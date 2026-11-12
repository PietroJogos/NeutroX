#!/usr/bin/env python3
"""Neutro X - instala o NeutroX.xex e os sons originais LOCAIS no console por FTP.

Uso (no PC, com o console ligado no Aurora, que serve FTP):
    python tools/xbox_install.py --host 192.168.0.50
    python tools/xbox_install.py --host 192.168.0.50 --xex dist/NeutroX-0.9.0-beta-xbox360/NeutroX.xex
    python tools/xbox_install.py --host 192.168.0.50 --dry-run     (so mostra o que faria)

O que faz:
  - cria a pasta de destino (padrao /Hdd1/Apps/NeutroX) se nao existir;
  - envia NeutroX.xex;
  - envia local_audio/<versao>/ (WAV + mapeamento.ini, ausentes.ini, manifesto.ini,
    relatorio.txt) gerado no PC por tools/sound_extract.py. A pasta brutos/ (copias
    intermediarias da extracao) NAO e enviada;
  - opcional: --covers envia capas_importar/ para importar no menu Meus Jogos.
  - --prepare-local PASTA prepara os mesmos arquivos para copiar sem FTP.

O que NAO faz: nao apaga nada no console, nao grava fora da pasta de destino, nao
toca em NAND, sistema, DashLaunch ou Aurora, e nao guarda a senha (pedida na hora,
ou --password para automatizar por sua conta).
"""
import argparse
import ftplib
import getpass
import os
from pathlib import Path
import posixpath
import shutil
import struct
import sys

VERSIONS = ["blades_6717", "nxe_9199", "kinect_13604", "metro_v1_15574", "metro_v2_17559"]
AUDIO_EXT = {".wav", ".ini", ".txt"}
EFFECTS = ("navegar", "confirmar", "voltar", "categoria", "abrir", "erro", "entrada", "transicao")


def default_local_audio():
    here = Path(__file__).resolve().parent
    for directory in (here / "local_audio", here.parent / "local_audio"):
        if directory.is_dir():
            return str(directory)
    base = os.environ.get("LOCALAPPDATA") or os.path.expanduser("~")
    return os.path.join(base, "NeutroX", "local_audio")


def default_xex():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    candidates = [os.path.join(here, "NeutroX.xex"), os.path.join(root, "NeutroX.xex")]
    dist = os.path.join(root, "dist")
    if os.path.isdir(dist):
        for d in sorted(os.listdir(dist), reverse=True):
            candidates.append(os.path.join(dist, d, "NeutroX.xex"))
    for c in candidates:
        if os.path.isfile(c):
            return c
    return None


def validate_wav(path):
    size = path.stat().st_size
    if size > 16 * 1024 * 1024:
        raise ValueError(f"WAV maior que 16 MiB: {path}")
    with path.open("rb") as f:
        header = f.read(12)
        if len(header) < 12 or header[:4] != b"RIFF" or header[8:] != b"WAVE":
            raise ValueError(f"WAV inválido: {path}")
        end = struct.unpack_from("<I", header, 4)[0] + 8
        fmt = None
        data_bytes = 0
        if end > size:
            raise ValueError(f"WAV truncado: {path}")
        while f.tell() + 8 <= end:
            key, length = struct.unpack("<4sI", f.read(8))
            next_chunk = f.tell() + length + (length & 1)
            if next_chunk > end:
                raise ValueError(f"bloco WAV truncado: {path}")
            if key == b"fmt " and length >= 16:
                raw = f.read(min(length, 40))
                tag, channels, rate, _, _, bits = struct.unpack_from("<HHIIHH", raw)
                if tag == 0xFFFE and len(raw) >= 40:
                    tag = struct.unpack_from("<H", raw, 24)[0]
                fmt = tag, channels, rate, bits
            elif key == b"data":
                data_bytes = length
            f.seek(next_chunk)
        if f.tell() != end or fmt is None or not data_bytes:
            raise ValueError(f"estrutura WAV inválida: {path}")
    tag, channels, rate, bits = fmt
    if not ((tag == 1 and bits in (8, 16, 24)) or (tag == 3 and bits == 32)):
        raise ValueError(f"codec WAV incompatível: {path}")
    if not 1 <= channels <= 8 or not 4000 <= rate <= 192000:
        raise ValueError(f"canais ou taxa WAV inválidos: {path}")
    block = channels * (bits // 8)
    if data_bytes % block or data_bytes > rate * block * 12:
        raise ValueError(f"alinhamento ou duração WAV inválida: {path}")


def audio_files(root):
    if not root.is_dir():
        raise ValueError(f"pasta de sons ausente: {root}. Use --local-audio ou confirme --no-audio.")
    result = []
    total = 0
    for version in VERSIONS:
        directory = root / version
        if not directory.is_dir():
            print(f"{version}: 0/8 efeitos, pasta ausente")
            continue
        files = {}
        for path in sorted(directory.iterdir()):
            if not path.is_file() or path.suffix.lower() not in AUDIO_EXT:
                continue
            if path.name.lower() in files:
                raise ValueError(f"nomes conflitantes no FATX: {path}")
            files[path.name.lower()] = path
        mapping = {}
        ini = files.get("mapeamento.ini")
        if ini:
            for line in ini.read_text(encoding="utf-8-sig").splitlines():
                key, _, value = line.partition("=")
                if key.strip() in EFFECTS:
                    mapping[key.strip()] = value.strip()
        loaded = 0
        for effect in EFFECTS:
            name = mapping.get(effect, effect + ".wav")
            if effect in mapping and (not name or Path(name).name != name or "/" in name or chr(92) in name):
                raise ValueError(f"nome incompatível no mapeamento: {version}/{name}")
            wav = files.get(name.lower())
            if wav:
                validate_wav(wav)
                loaded += 1
            elif effect in mapping:
                raise ValueError(f"mapeamento aponta para WAV ausente: {version}/{name}")
        wav_count = sum(p.suffix.lower() == ".wav" for p in files.values())
        print(f"{version}: {loaded}/8 efeitos, {wav_count} WAV")
        total += loaded
        for p in files.values():
            if len(p.name.encode("utf-8")) > 42:
                raise ValueError(f"nome excede 42 bytes no FATX: {p.name}")
            result.append((p, posixpath.join("local_audio", version, p.name)))
    if not total:
        raise ValueError("nenhum efeito original válido encontrado; use --no-audio para instalar somente o aplicativo.")
    return result


def plan(args):
    """Lista (local, remoto) a enviar."""
    items = []
    if args.xex:
        items.append((args.xex, posixpath.join(args.dest, "NeutroX.xex")))
    if not args.no_audio:
        for path, name in audio_files(Path(args.local_audio)):
            items.append((str(path), posixpath.join(args.dest, name)))
    if args.covers:
        cd = args.covers
        for name in sorted(os.listdir(cd)) if os.path.isdir(cd) else []:
            p = os.path.join(cd, name)
            if os.path.isfile(p) and os.path.splitext(name)[1].lower() in {".jpg", ".jpeg", ".png"}:
                items.append((p, posixpath.join(args.dest, "capas_importar", name)))
    return items


def ensure_dirs(ftp, path, made):
    parts = [p for p in path.split("/") if p]
    cur = ""
    for p in parts:
        cur += "/" + p
        if cur in made:
            continue
        try:
            ftp.cwd(cur)
        except ftplib.error_perm:
            ftp.mkd(cur)
        made.add(cur)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", help="IP do console (tela do Aurora)")
    ap.add_argument("--port", type=int, default=21)
    ap.add_argument("--user", default="xboxftp", help="usuario FTP do Aurora (padrao xboxftp)")
    ap.add_argument("--password", default=None, help="senha FTP (se omitida, e pedida)")
    ap.add_argument("--dest", default="/Hdd1/Apps/NeutroX", help="pasta do Neutro X no console")
    ap.add_argument("--xex", default=None, help="NeutroX.xex a enviar (padrao: o mais recente em dist/)")
    ap.add_argument("--local-audio", default=default_local_audio())
    ap.add_argument("--no-audio", action="store_true", help="nao enviar local_audio/")
    ap.add_argument("--covers", default=None, help="pasta de capas no PC para enviar a capas/")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--prepare-local", type=Path, metavar="PASTA",
                    help="monta a instalação local para copiar à pasta do XEX no console")
    args = ap.parse_args()
    if not (args.host or args.prepare_local or args.dry_run):
        ap.error("informe --host, --prepare-local ou --dry-run")
    if args.xex is None:
        args.xex = default_xex()
        if args.xex is None:
            print("AVISO: NeutroX.xex nao encontrado; use --xex. Enviando apenas os sons.")
    args.dest = "/" + args.dest.strip("/")

    if args.prepare_local and not args.xex:
        ap.error("a instalação local precisa de um NeutroX.xex (--xex)")
    try:
        items = plan(args)
    except (OSError, ValueError, struct.error) as e:
        ap.exit(2, f"Erro: {e}\n")
    if not items:
        print("Nada a enviar.")
        return 1
    total = sum(os.path.getsize(l) for l, _ in items)
    target = args.prepare_local or (f"ftp://{args.host}{args.dest}" if args.host else args.dest)
    print(f"{len(items)} arquivos, {total / 1048576:.1f} MB -> {target}")
    if args.dry_run:
        for l, r in items:
            print(f"  {l} -> {r}")
        return 0

    if args.prepare_local:
        for local, remote in items:
            target = args.prepare_local / remote.removeprefix(args.dest).lstrip("/")
            target.parent.mkdir(parents=True, exist_ok=True)
            if Path(local).resolve() != target.resolve():
                shutil.copy2(local, target)
        return 0

    pw = args.password if args.password is not None else getpass.getpass(f"Senha FTP de {args.user}@{args.host}: ")
    ftp = ftplib.FTP()
    ftp.connect(args.host, args.port, timeout=20)
    ftp.login(args.user, pw)
    made = set()
    sent = 0
    for local, remote in items:
        ensure_dirs(ftp, posixpath.dirname(remote), made)
        with open(local, "rb") as f:
            ftp.storbinary("STOR " + remote, f, blocksize=65536)
        try:
            size = ftp.size(remote)
        except ftplib.error_perm as e:
            if str(e)[:3] not in {"500", "501", "502", "504"}:
                raise
        else:
            if size != os.path.getsize(local):
                raise OSError(f"tamanho divergente depois da transferência: {remote}")
        sent += 1
        print(f"  [{sent}/{len(items)}] {remote}")
    ftp.quit()
    print("Concluido. Abra o NeutroX.xex pelo Aurora; em Configuracoes > Sons originais: estado confira o que carregou.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
