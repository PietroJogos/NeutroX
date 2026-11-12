"""Inventário de fontes locais e consolidação do catálogo de áudio por versão."""
from collections import defaultdict
from dataclasses import dataclass
import hashlib
import json
import re
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import tempfile
import zipfile

LIMIT = 128 * 1024 * 1024
RESOURCE = {".xzp", ".xuiz", ".xur", ".xui", ".xwb", ".xsb", ".xma", ".wav", ".wma"}
DASH_EXT = {".xex", ".lex"}


@dataclass(frozen=True)
class Entry:
    path: Path
    relative: PurePosixPath
    origin: str

    @property
    def name(self):
        return self.relative.name

    @property
    def ext(self):
        return self.relative.suffix.lower()


def file_hash(path):
    with path.open("rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()


def archive(path, work, notes):
    with zipfile.ZipFile(path) as z:
        notes.append(f"ZIP {path.name}: {len(z.infolist())} entradas, SHA-256 {file_hash(path)}")
        for i, member in enumerate(z.infolist()):
            relative = PurePosixPath(member.filename.replace(chr(92), "/"))
            if member.is_dir() or relative.suffix.lower() not in DASH_EXT | RESOURCE:
                continue
            origin = f"{path.name}!/{relative}"
            if member.flag_bits & 1 or member.file_size > LIMIT:
                notes.append(f"  [indisponível] {origin}: criptografado ou acima de 128 MiB")
                continue
            if relative.is_absolute() or ".." in relative.parts:
                notes.append(f"  [ignorado] nome inválido em {origin!r}")
                continue
            copied = work / f"entrada-{i}{relative.suffix.lower()}"
            with z.open(member) as f, copied.open("wb") as out:
                shutil.copyfileobj(f, out)
            yield Entry(copied, relative, origin)


def groups(path, work, notes, excluded):
    if path.suffix.lower() == ".zip" and path.is_file():
        work.mkdir()
        yield list(archive(path, work, notes))
    elif path.is_file():
        yield [Entry(path, PurePosixPath(path.name), path.name)]
    else:
        loose = []
        for p in sorted(path.rglob("*")):
            if not p.is_file() or any(p.resolve().is_relative_to(e) for e in excluded):
                continue
            if p.suffix.lower() == ".zip":
                # O ZIP preserva seu próprio contexto para atribuir a versão.
                work.mkdir(exist_ok=True)
                sub = Path(tempfile.mkdtemp(prefix="zip-", dir=work))
                yield list(archive(p, sub, notes))
            elif p.suffix.lower() in DASH_EXT | RESOURCE:
                rel = PurePosixPath(p.relative_to(path).as_posix())
                loose.append(Entry(p, rel, f"{path.name}/{rel}"))
        yield loose


def inventory(entries, declared, api, notes):
    dashboards = []
    auxiliaries = []
    resources = []
    parents = defaultdict(set)
    for entry in entries:
        if entry.path.stat().st_size > LIMIT:
            notes.append(f"[indisponível] {entry.origin}: acima de 128 MiB")
            continue
        if entry.ext in DASH_EXT:
            info = api.xex_info(entry.path)
            if info:
                dashboards.append((entry, info))
                parents[entry.relative.parent].add(info["build"])
                notes.append(f"[dashboard] {entry.origin}: {info['version']}; criptografia {info['encrypted']}")
            elif re.match(r"^(dash|xam|hud|nuihud|boot|guide\.)", entry.name, re.IGNORECASE):
                header = api.xex_inspect.inspect(str(entry.path))
                version = header.get("execution_id", {}).get("version", "")
                fmt = header.get("file_format", {})
                notes.append(f"[módulo auxiliar] {entry.origin}: {version or 'sem versão XEX2'}; {fmt}")
                match = re.fullmatch(r"2\.0\.(\d+)\.0", version)
                if (match and int(match[1]) in api.THEMES and not header["errors"] and
                        fmt.get("encryption") == "nenhuma"):
                    auxiliaries.append((entry, {"build": int(match[1]), "version": version,
                                                "encrypted": False, "header": header}))
            continue
        with entry.path.open("rb") as f:
            magic = f.read(4)
        if magic in (b"BDES", b"XUIZ") or entry.ext in {".xzp", ".xuiz"}:
            notes.append(f"[recurso separado] {entry.origin}: assinatura {magic!r}")
        if magic == b"XUIZ" or entry.ext in RESOURCE - {".xzp", ".xuiz"}:
            resources.append(entry)

    all_builds = set().union(*parents.values())
    associated = defaultdict(list)
    for entry in resources:
        builds = next((parents[p] for p in entry.relative.parents if p in parents), all_builds)
        if not builds and declared:
            builds = {declared}
        if len(builds) != 1:
            notes.append(f"[origem indeterminada] {entry.origin}: sem versão única verificável")
            continue
        build, = builds
        associated[build].append(entry)
        notes.append(f"[recurso {build}] {entry.origin}" + (" (versão declarada)" if not all_builds else ""))
    return dashboards, associated, auxiliaries


def atomic_copy(source, target):
    if target.exists() and file_hash(source) == file_hash(target):
        return
    with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as out:
        temp = Path(out.name)
        with source.open("rb") as f:
            shutil.copyfileobj(f, out)
    try:
        temp.replace(target)
    finally:
        temp.unlink(missing_ok=True)


def write_text(path, content):
    with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=path.parent, delete=False) as f:
        f.write(content)
        temp = Path(f.name)
    try:
        temp.replace(path)
    finally:
        temp.unlink(missing_ok=True)


def merge(api, build, runs, dest, auxiliary=False):
    folder, label = api.THEMES[build]
    target = dest / folder
    target.mkdir(exist_ok=True)
    if target.resolve() != target:
        raise ValueError(f"pasta de versão redirecionada: {target}")
    index = {}
    selected = defaultdict(set)
    causes = defaultdict(list)
    external = set()
    errors = []
    calls = []
    for result, scratch in runs:
        source = result["fonte"]
        external.update(result["recursos_externos"])
        errors.extend(result["erros_conversao"])
        for call in result["chamadas_lua"]:
            if call not in calls:
                calls.append(call)
        names = {}
        for row in result["arquivos"]:
            digest = row["validacao"]["sha256"]
            names[row["arquivo"]] = digest
            if digest not in index:
                installed = target / row["arquivo"]
                if installed.exists() and file_hash(installed) != digest:
                    installed = target / (api.safe_stem(installed.stem) + "_" + digest[:10] + ".wav")
                    if installed.exists() and file_hash(installed) != digest:
                        raise ValueError(f"colisão no arquivo {installed}")
                atomic_copy(scratch / folder / row["arquivo"], installed)
                index[digest] = {**row, "arquivo": installed.name, "usado_para": [], "proveniencia": []}
            index[digest]["proveniencia"].append({"fonte": source, "sha256_fonte": result["sha256_fonte"],
                                                  "recurso": row["origem"], "sha256_recurso": row["sha256_recurso"],
                                                  "evidencia": row["evidencia"]})
        for effect, name in result["efeitos"].items():
            if name is not None and not auxiliary:
                selected[effect].add(names[name])
        for effect, why in result["ausentes"].items():
            if why not in causes[effect]:
                causes[effect].append(why)

    mapping, absent = {}, {}
    for effect in api.EFFECTS:
        choices = selected[effect]
        if len(choices) == 1:
            chosen = index[next(iter(choices))]
            mapping[effect] = chosen["arquivo"]
            chosen["usado_para"].append(effect)
            chosen["status"] = "confirmado"
        elif choices:
            absent[effect] = "Fontes da mesma versão indicam WAVs diferentes para esta função"
        else:
            why = causes[effect]
            absent[effect] = next((m for m in why if m.startswith("nenhuma")), why[0] if why
                                  else "Sem fonte desta versão no lote")
    for row in index.values():
        if row["status"] == "confirmado" and not row["usado_para"]:
            row["status"] = "alternativa"
    if not auxiliary:
        manual = target / "mapeamento.ini"
        generated = target / "mapeamento.extraido.ini"
        text = api.GENERATED_TAG + "\n" + "".join(f"{e}={f}\n" for e, f in mapping.items())
        if not manual.exists() or manual.read_text(encoding="utf-8-sig").startswith(api.GENERATED_TAG):
            write_text(manual, text)
        write_text(generated, text)
        write_text(target / "ausentes.ini", api.GENERATED_TAG + "\n" +
                   "".join(f"{e}={m}\n" for e, m in absent.items()))
    states = sorted({result["xex_estado"] for result, _ in runs})
    unknown = sum(r["status"] == "nao_confirmado" for r in index.values())
    manifest = [api.GENERATED_TAG, f"dashboard={label}", f"versao=2.0.{build}.0",
                f"xex_estado={', '.join(states) or 'ausente'}", f"audios_originais={len(index)}",
                f"efeitos_esperados={len(api.EFFECTS)}", f"efeitos_confirmados={len(mapping)}",
                f"nao_confirmados={unknown}", f"erros_conversao={len(errors)}",
                f"recursos_externos={len(external)}", f"situacao={len(mapping)} de {len(api.EFFECTS)} efeitos confirmados",
                "# arquivo.<nome>=status|efeito|origem|codec|canais|taxa|segundos|sha256"]
    for digest, row in index.items():
        v = row["validacao"]
        manifest.append(f"arquivo.{row['arquivo']}={row['status']}|{','.join(row['usado_para']) or '-'}|"
                        f"{row['origem']}|{row['codec']}|{v['canais']}|{v['taxa']}|{v['segundos']}|{digest}")
    write_text(target / "manifesto.ini", "\n".join(manifest) + "\n")
    return {"dashboard": label, "versao": f"2.0.{build}.0", "pasta": folder, "estados": states,
            "efeitos": {effect: mapping.get(effect) for effect in api.EFFECTS}, "ausentes": absent,
            "arquivos": list(index.values()), "recursos_externos": sorted(external), "erros_conversao": errors,
            "chamadas_lua": calls}


def run_batch(args, api, unpack, ffmpeg):
    dest = Path(args.destino).resolve()
    dest.mkdir(parents=True, exist_ok=True)
    inputs = [(Path(name).resolve(strict=True), None) for name in args.entrada]
    for spec in args.recursos:
        build, sep, path = spec.partition("=")
        if not sep or not build.isdecimal() or int(build) not in api.THEMES:
            raise ValueError("--recursos precisa de BUILD=CAMINHO")
        inputs.append((Path(path).resolve(strict=True), int(build)))
    excluded = {dest}
    if args.pacote_local:
        excluded.add(Path(args.pacote_local).resolve())
    notes = []
    summaries = []
    with tempfile.TemporaryDirectory(dir=dest) as temporary:
        scratch = Path(temporary)
        candidates, associated, auxiliaries = defaultdict(list), defaultdict(list), defaultdict(list)
        for i, (path, declared) in enumerate(inputs):
            if any(path.is_relative_to(e) for e in excluded):
                raise ValueError(f"entrada dentro do destino: {path}")
            for group in groups(path, scratch / f"input-{i}", notes, excluded):
                dashboards, resources, modules = inventory(group, declared, api, notes)
                for entry, info in modules:
                    auxiliaries[info["build"]].append((entry, info))
                for entry, info in dashboards:
                    candidates[info["build"]].append((entry, info))
                for build, entries in resources.items():
                    associated[build].extend(entries)
        for build, (folder, label) in api.THEMES.items():
            runs, details, seen = [], [], set()
            sources = sorted(candidates[build], key=lambda pair: pair[1]["encrypted"] is not False)
            for entry, info in sources:
                digest = file_hash(entry.path)
                if digest in seen:
                    notes.append(f"[mesmo XEX] {entry.origin}: SHA-256 {digest}")
                    continue
                seen.add(digest)
                work = scratch / f"{folder}-{len(runs)}"
                result, lines = api.process_version(entry.path, info, work, unpack, ffmpeg, origin=entry.origin)
                runs.append((result, work))
                details.extend(lines)
            if associated[build]:
                work = scratch / f"{folder}-avulsos"
                result, lines = api.process_version(None, {"build": build, "version": f"2.0.{build}.0"},
                                                    work, unpack, ffmpeg, extras=associated[build])
                runs.append((result, work))
                details.extend(lines)
            # Uma reexecução parcial mantém catálogos de versões fora do lote.
            if not runs and (dest / folder / "manifesto.ini").exists():
                notes.append(f"[catálogo anterior preservado] {label} {build}: sem nova fonte")
                continue
            combined = merge(api, build, runs, dest)
            summaries.append(combined)
            write_text(dest / folder / "relatorio.txt", "\n".join(details or ["Sem fonte desta versão no lote"]) + "\n")
        auxiliary_summaries = []
        for build, modules in sorted(auxiliaries.items()):
            folder, label = api.THEMES[build]
            details, runs, seen = [], [], set()
            for entry, info in modules:
                digest = file_hash(entry.path)
                if digest in seen:
                    continue
                seen.add(digest)
                work = scratch / f"aux-{folder}-{len(runs)}"
                result, lines = api.process_version(entry.path, info, work, unpack, ffmpeg, origin=entry.origin)
                runs.append((result, work))
                details.extend(lines)
            out = dest / "auxiliares"
            out.mkdir(exist_ok=True)
            combined = merge(api, build, runs, out, auxiliary=True)
            auxiliary_summaries.append(combined)
            write_text(out / folder / "relatorio.txt", "\n".join(details) + "\n")
            notes.append(f"[auxiliar {build}] {label}: {len(runs)} imagens em texto claro, "
                         f"{len(combined['arquivos'])} WAVs de módulos, sem vínculo com os slots do dashboard")
    write_text(dest / "manifesto.json", json.dumps({"versoes": summaries, "auxiliares": auxiliary_summaries, "inventario": notes}, ensure_ascii=False, indent=2))
    lines = [*notes, "", "RESUMO POR VERSÃO"]
    for result in summaries:
        count = sum(v is not None for v in result["efeitos"].values())
        lines.append(f"{result['dashboard']} {result['versao']}: {len(result['arquivos'])} WAV, "
                     f"{count}/{len(api.EFFECTS)} efeitos, {len(result['erros_conversao'])} falhas de conversão")
        lines.extend(f"  {effect}: {why}" for effect, why in result["ausentes"].items())
        lines.extend(f"  [externo] {ref}" for ref in result["recursos_externos"])
    write_text(dest / "relatorio_extracao.txt", "\n".join(lines) + "\n")
    print("\n".join(lines[-min(len(lines), 60):]))
    if args.pacote_local:
        installer = Path(__file__).with_name("xbox_install.py")
        code = subprocess.run([sys.executable, str(installer), "--prepare-local", args.pacote_local,
                               "--xex", args.xex_app, "--local-audio", str(dest)]).returncode
        if code:
            return code
        diagnostic = Path(args.pacote_local) / "diagnostico"
        diagnostic.mkdir(exist_ok=True)
        for filename in ("manifesto.json", "relatorio_extracao.txt"):
            atomic_copy(dest / filename, diagnostic / filename)
        if auxiliary_summaries:
            shutil.copytree(dest / "auxiliares", diagnostic / "auxiliares", dirs_exist_ok=True)
    return 0
