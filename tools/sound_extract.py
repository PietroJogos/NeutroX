#!/usr/bin/env python3
"""
sound_extract.py - Extracao em lote dos sons ORIGINAIS dos dashboards (uso local).

O que faz, para cada arquivo .xex informado (ou encontrado na pasta de entrada):
  1. Le o cabecalho XEX2 e identifica a versao pelo Execution ID (ex.: 2.0.17559.0).
  2. Se o corpo estiver CRIPTOGRAFADO, recusa e registra o motivo (abrir exigiria
     chaves do console = contornar protecao; isso nao e feito).
  3. Se nao estiver, extrai a imagem com neutrox_xex_unpack (LZX, somente leitura),
     percorre os pacotes de recursos XUIZ e localiza arquivos de audio embutidos
     (RIFF/WAVE, inclusive XMA).
  4. Converte para WAV PCM 16 bits com o FFmpeg, mantendo taxa e canais; guarda
     tambem os originais sem conversao em <destino>/brutos/.
  5. Associa funcoes (navegar, confirmar...) SOMENTE por evidencia verificavel: o
     nome original do arquivo dentro do pacote. Sem evidencia, o efeito fica
     ausente e a causa e registrada (nada e inventado nem trocado por recriacao).
  6. Instala em <destino>/<versao>/ (ex.: metro_v2_17559/) com mapeamento.ini,
     ausentes.ini e relatorio; o Neutro X reconhece tudo automaticamente.
Tambem procura arquivos externos de som (.xwb/.xsb/.xma/.wav) na pasta de entrada.

O material extraido e PROPRIETARIO: fica apenas na pasta local do usuario,
fora do repositorio (o .gitignore exclui sons_originais/).

Uso:
  python tools/sound_extract.py --entrada <pasta_com_xex> [--destino <pasta>]
         [--unpack <neutrox_xex_unpack.exe>] [--ffmpeg <ffmpeg.exe>]
  (destino padrao: %LOCALAPPDATA%/NeutroX/sons_originais)
"""
import argparse
import datetime
import os
import pathlib
import struct
import subprocess
import sys
import tempfile

# versao do dashboard (build) -> pasta de tema do Neutro X
THEMES = {
    6717: ("blades_6717", "Blades"),
    9199: ("nxe_9199", "NXE"),
    13604: ("kinect_13604", "Kinect"),
    15574: ("metro_v1_15574", "Metro V1"),
    17559: ("metro_v2_17559", "Metro V2"),
}
EFFECTS = ["navegar", "confirmar", "voltar", "categoria", "abrir", "erro", "entrada", "transicao"]

# Regras de evidencia: nome original (sem extensao, minusculas) -> efeito do Neutro X.
# Somente nomes cujo significado corresponde diretamente a funcao.
NAME_RULES = [
    ("btn_focus", "navegar", "foco do botao (mover selecao)"),
    ("snd_panelright", "navegar", "movimento entre paineis (direita)"),
    ("btn_select", "confirmar", "botao selecionar"),
    ("snd_buttonselect", "confirmar", "botao selecionar"),
    ("btn_back", "voltar", "botao voltar"),
    ("snd_buttonback", "voltar", "botao voltar"),
    ("tab_switch", "categoria", "troca de aba/categoria"),
    ("snd_channelup", "categoria", "troca de canal/categoria"),
    ("snd_panelunfold", "abrir", "painel se abrindo"),
    ("btn_inactiveselect", "erro", "selecao de item inativo"),
    ("snd_transitionfrom", "transicao", "transicao de tela"),
]
AUDIO_EXT = (".xma", ".wav", ".wma")


def be32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def xex_info(path):
    d = open(path, "rb").read(0x10000)
    if d[:4] != b"XEX2":
        return None
    count = be32(d, 0x14)
    info = {"build": None, "version": None, "encrypted": None}
    for i in range(count):
        k, v = struct.unpack_from(">II", d, 0x18 + i * 8)
        if k == 0x00040006:
            ver = be32(d, v + 4)
            info["build"] = (ver >> 8) & 0xFFFF
            info["version"] = f"{(ver >> 28) & 0xF}.{(ver >> 24) & 0xF}.{(ver >> 8) & 0xFFFF}.{ver & 0xFF}"
        if k == 0x000003FF:
            info["encrypted"] = struct.unpack_from(">H", d, v + 4)[0] != 0
    return info


def parse_xuiz(blob):
    """Pacote de recursos XUIZ: cabecalho de 22 bytes + tabela [tam u32][desl u32][len u8][nome] + dados."""
    if blob[:4] != b"XUIZ" or len(blob) < 22:
        return []
    count = struct.unpack_from(">H", blob, 0x14)[0]
    p = 0x16
    entries = []
    for _ in range(count):
        if p + 9 > len(blob):
            return []
        size, off = struct.unpack_from(">II", blob, p)
        n = blob[p + 8]
        name = blob[p + 9:p + 9 + n].decode("latin-1")
        entries.append((name, size, off))
        p += 9 + n
    base = p  # dados comecam logo apos a tabela
    out = []
    for name, size, off in entries:
        a = base + off
        if a + size <= len(blob):
            out.append((name, blob[a:a + size]))
    return out


def classify(name):
    """Retorna (efeito, motivo, prioridade); regras anteriores na lista tem prioridade."""
    stem = pathlib.PurePosixPath(name.replace("\\", "/")).stem.lower()
    for rank, (key, eff, why) in enumerate(NAME_RULES):
        if stem == key:
            return eff, why, rank
    return None, None, None


def find_ffmpeg(explicit):
    """Escolhe um ffmpeg que realmente tenha decodificador XMA (ha wrappers com o mesmo nome no PATH)."""
    cands = [explicit] if explicit else []
    for d in os.environ.get("PATH", "").split(os.pathsep):
        p = pathlib.Path(d) / ("ffmpeg.exe" if os.name == "nt" else "ffmpeg")
        if p.exists():
            cands.append(str(p))
    winget = pathlib.Path(os.environ.get("LOCALAPPDATA", "")) / "Microsoft" / "WinGet" / "Packages"
    if winget.exists():
        cands += [str(p) for p in winget.glob("*FFmpeg*/**/bin/ffmpeg.exe")]
    for c in cands:
        try:
            r = subprocess.run([c, "-hide_banner", "-decoders"], capture_output=True, text=True, timeout=20)
            if r.returncode == 0 and "xma1" in r.stdout:
                return c
        except (OSError, subprocess.SubprocessError):
            continue
    return None


def convert(ffmpeg, src, dst):
    r = subprocess.run([ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-i", str(src), "-c:a", "pcm_s16le", str(dst)],
                       capture_output=True, text=True)
    return r.returncode == 0 and dst.exists() and dst.stat().st_size > 44, r.stderr.strip()


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--entrada", required=True)
    ap.add_argument("--destino", default=os.path.join(os.environ.get("LOCALAPPDATA", "."), "NeutroX", "sons_originais"))
    ap.add_argument("--unpack", default=None)
    ap.add_argument("--ffmpeg", default=None)
    a = ap.parse_args(argv)

    here = pathlib.Path(__file__).resolve().parent.parent
    unpack = a.unpack or next((str(p) for p in [here / "build_release/Release/neutrox_xex_unpack.exe",
                                                 here / "build/Release/neutrox_xex_unpack.exe"] if p.exists()), None)
    if not unpack:
        print("ERRO: neutrox_xex_unpack nao encontrado (compile o projeto ou use --unpack)")
        return 2
    a.ffmpeg = find_ffmpeg(a.ffmpeg)
    if not a.ffmpeg:
        print("ERRO: nenhum ffmpeg com decodificador XMA encontrado (instale o FFmpeg completo ou use --ffmpeg)")
        return 2
    src = pathlib.Path(a.entrada)
    dest = pathlib.Path(a.destino)
    dest.mkdir(parents=True, exist_ok=True)
    report = [f"Neutro X - extracao de sons originais ({datetime.datetime.now():%d/%m/%Y %H:%M})",
              f"Entrada: {src}", f"Destino: {dest}", f"Conversor: {a.ffmpeg}", ""]
    seen = {}

    xexs = sorted(src.rglob("*.xex"))
    for x in xexs:
        info = xex_info(x)
        if not info or info["build"] not in THEMES:
            report.append(f"[ignorado] {x.name}: nao e um dashboard conhecido")
            continue
        folder, label = THEMES[info["build"]]
        tdir = dest / folder
        tdir.mkdir(parents=True, exist_ok=True)
        found = {}      # efeito -> (arquivo wav, origem, motivo)
        extras = []     # audios sem funcao identificada
        causes = {}
        report.append(f"== {label} ({info['version']}) - {x.name}")
        if info["encrypted"]:
            cause = ("XEX criptografado: abri-lo exigiria chaves do console (contornar protecao); "
                     "nao extraido")
            for e in EFFECTS:
                causes[e] = cause
            report.append("   " + cause)
        else:
            with tempfile.TemporaryDirectory() as tmp:
                r = subprocess.run([unpack, str(x), tmp], capture_output=True, text=True)
                if r.returncode != 0:
                    cause = "falha ao extrair a imagem do XEX: " + (r.stderr.strip() or r.stdout.strip())[:120]
                    for e in EFFECTS:
                        causes[e] = cause
                    report.append("   " + cause)
                else:
                    raw_dir = dest / "brutos" / folder
                    raw_dir.mkdir(parents=True, exist_ok=True)
                    referenced = set()
                    for res in sorted(pathlib.Path(tmp, "recursos").glob("*.bin")):
                        blob = res.read_bytes()
                        for name, data in parse_xuiz(blob):
                            low = name.lower()
                            if low.endswith((".xsb", ".xwb")):
                                referenced.add(pathlib.PurePosixPath(name.replace("\\", "/")).name)
                            if not low.endswith(AUDIO_EXT):
                                continue
                            stem = pathlib.PurePosixPath(name.replace("\\", "/")).stem
                            if data[:4] != b"RIFF":
                                report.append(f"   [referencia] {res.stem}/{name}: sem dados de audio no pacote")
                                continue
                            raw = raw_dir / f"{res.stem}__{stem}{pathlib.PurePosixPath(name).suffix.lower()}"
                            raw.write_bytes(data)
                            wav = tdir / f"{stem}.wav"
                            ok, err = convert(a.ffmpeg, raw, wav)
                            if not ok:
                                report.append(f"   [falha] {res.stem}/{name}: conversao falhou ({err[:80]})")
                                continue
                            eff, why, rank = classify(name)
                            line = f"   [audio] {res.stem}/{name} -> {wav.name}"
                            if eff and (eff not in found or rank < found[eff][3]):
                                if eff in found:
                                    extras.append(found[eff][0])  # regra mais fraca vira alternativa
                                found[eff] = (wav.name, f"{res.stem}/{name}", why, rank)
                                line += f"  => {eff} (evidencia: nome '{stem}' = {why})"
                            else:
                                extras.append(wav.name)
                                line += "  (sem funcao identificada: guardado na pasta)" if not eff else f"  (alternativa para {eff})"
                            report.append(line)
                    for ref in sorted(referenced):
                        report.append(f"   [externo] o XEX referencia '{ref}', que fica FORA do XEX (nao fornecido)")
                    for e in EFFECTS:
                        if e not in found:
                            if referenced:
                                causes[e] = ("sem audio com esse nome no XEX; os sons principais ficam no banco externo " +
                                             "/".join(sorted(referenced)) + ", nao fornecido")
                            else:
                                causes[e] = "nenhum audio com nome correspondente dentro do XEX"
        # arquivos externos fornecidos pelo usuario (mesma pasta do XEX)
        for ext in sorted(set(x.parent.glob("*.xwb")) | set(x.parent.glob("*.xsb"))):
            report.append(f"   [externo] {ext.name} encontrado: formato de banco XACT ainda nao suportado pelo extrator")
        # mapeamento e ausencias para o Neutro X
        (tdir / "mapeamento.ini").write_text(
            "# Gerado por tools/sound_extract.py - funcao identificada pelo nome original\n" +
            "".join(f"{e}={found[e][0]}\n" for e in EFFECTS if e in found), encoding="utf-8")
        (tdir / "ausentes.ini").write_text(
            "# Efeitos sem audio original e o motivo (exibido em Configuracoes > Sons originais)\n" +
            "".join(f"{e}={causes[e]}\n" for e in EFFECTS if e not in found), encoding="utf-8")
        seen[folder] = (label, info["version"], found, extras, causes)
        report.append(f"   resumo: {len(found)} de {len(EFFECTS)} efeitos identificados; {len(extras)} outros audios guardados")
        report.append("")

    for b, (folder, label) in THEMES.items():
        if folder not in seen:
            report.append(f"== {label}: nenhum XEX desta versao na entrada")
    text = "\n".join(report) + "\n"
    (dest / "relatorio_extracao.txt").write_text(text, encoding="utf-8")
    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
