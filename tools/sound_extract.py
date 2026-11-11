#!/usr/bin/env python3
"""
sound_extract.py - Inspecao e extracao em lote dos sons ORIGINAIS dos dashboards (uso local).

Para cada arquivo .xex informado (ou encontrado na pasta de entrada):
  1. Le o cabecalho XEX2 (texto claro): versao pelo Execution ID, criptografia,
     compressao, pacotes de recursos, bibliotecas estaticas e importadas
     (via tools/xex_inspect.py). Isso funciona ate em XEX criptografado.
  2. Se o corpo estiver CRIPTOGRAFADO, para aqui e registra o motivo: abri-lo
     exigiria as chaves do console, o que seria contornar uma protecao.
  3. Se nao estiver, extrai a imagem com neutrox_xex_unpack (LZX, somente
     leitura) e percorre os pacotes de recursos XUIZ procurando:
       - bancos XACT: dash.xwb (ondas) + dash.xsb (cues = nomes de uso);
       - arquivos RIFF/WAVE soltos (inclusive XMA);
       - cenas/skins XUI (.xur) que referenciam sons (evidencia de funcao);
       - referencias a recursos externos (sharedres:// = recursos do XAM).
  4. Converte para WAV PCM 16 bits com o FFmpeg (XMA/XMA2 -> PCM), mantendo
     canais e taxa, e VALIDA o resultado: canais, taxa, duracao (contra a
     declarada no banco), integridade e silencio.
  5. Associa funcoes (navegar, confirmar...) SOMENTE por evidencia verificavel:
     o nome original da cue/arquivo e a skin/cena que o usa. Sem evidencia, o
     audio fica como "nao confirmado" (guardado, mas nao usado). Nada e
     inventado nem trocado por recriacao.
  6. Instala em <destino>/<versao>/ com mapeamento.ini, ausentes.ini,
     manifesto.ini e relatorio.txt (um relatorio tecnico por versao), alem de
     <destino>/manifesto.json e <destino>/relatorio_extracao.txt.
Arquivos .xwb/.xsb soltos ao lado do XEX tambem sao processados.

O material extraido e PROPRIETARIO: fica apenas na pasta local do usuario,
fora do repositorio (o .gitignore exclui local_audio/ e sons_originais/).

Uso:
  python tools/sound_extract.py --entrada <pasta_com_xex> [--destino <pasta>]
         [--unpack <neutrox_xex_unpack.exe>] [--ffmpeg <ffmpeg.exe>]
  (destino padrao: %LOCALAPPDATA%/NeutroX/local_audio)
"""
import argparse
import datetime
import hashlib
import json
import os
import pathlib
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import wave

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
sys.dont_write_bytecode = True  # nao deixa __pycache__ em tools/
import xex_inspect  # noqa: E402  (mesmo diretorio)

GENERATED_TAG = "# Gerado por tools/sound_extract.py"

# versao do dashboard (build) -> pasta de tema do Neutro X
THEMES = {
    6717: ("blades_6717", "Blades"),
    9199: ("nxe_9199", "NXE"),
    13604: ("kinect_13604", "Kinect"),
    15574: ("metro_v1_15574", "Metro V1"),
    17559: ("metro_v2_17559", "Metro V2"),
}
EFFECTS = ["navegar", "confirmar", "voltar", "categoria", "abrir", "erro", "entrada", "transicao"]

# Evidencia de funcao: nome original (cue XACT ou arquivo, minusculas) -> efeito.
# Somente nomes cujo significado corresponde diretamente ao evento do Neutro X.
# A ordem e a prioridade (primeiro = preferido).
FUNCTION_RULES = [
    ("btn_focus", "navegar", "foco de botao: a selecao passa para outra opcao"),
    ("snd_buttonselect", "confirmar", "som de selecao de botao (selectSoundXACT da skin do dashboard)"),
    ("btn_select", "confirmar", "selecao de botao"),
    ("select_button", "confirmar", "selecao de botao"),
    ("snd_buttonback", "voltar", "som do botao voltar"),
    ("btn_back", "voltar", "botao voltar"),
    ("tab_switch", "categoria", "troca de aba/categoria"),
    ("snd_panelunfold", "abrir", "painel se abrindo (PanelFoldSounds)"),
    ("btn_inactiveselect", "erro", "selecao de item inativo (acao indisponivel)"),
    ("snd_transitionfrom", "transicao", "transicao de cena (SceneTransitions)"),
]
RULE_INDEX = {k: (e, why, rank) for rank, (k, e, why) in enumerate(FUNCTION_RULES)}

# Funcao conhecida pelo nome, mas sem evento equivalente no Neutro X (guardado).
KNOWN_OTHER = {
    "snd_panelfold": "fechamento de painel",
    "dl_complete": "notificacao: download concluido",
    "btn_inactivefocus": "foco em item inativo",
    "snd_channelup": "troca de canal (para cima)",
    "snd_channeldown": "troca de canal (para baixo)",
    "snd_panelleft": "painel para a esquerda",
    "snd_panelright": "painel para a direita",
    "snd_pageleft": "pagina para a esquerda",
    "snd_pageright": "pagina para a direita",
    "vk_focus": "teclado virtual: foco",
    "vk_select": "teclado virtual: selecao",
    "sign_in_focus": "tela de perfis: foco",
    "sign_in_select": "tela de perfis: selecao",
    "social_focus": "area social: foco",
    "social_select": "area social: selecao",
    "social_modal_focus": "janela social: foco",
    "progress_meter": "medidor de progresso",
    "wave_success": "Kinect: aceno reconhecido",
    "engage": "Kinect: jogador detectado",
    "disengage": "Kinect: jogador saiu",
    "handle_grab_1": "Kinect: alca agarrada",
    "control_kinect": "Kinect: controle",
}
AUDIO_EXT = (".xma", ".wav", ".wma")
XMA_RATES = (24000, 32000, 44100, 48000)
AUDIO_LIBS = ("XAUD", "XACT", "XMA", "XHV", "X3DAUDIO", "XAUDIO2", "XMCORE")
MAX_SECONDS = 12.0  # limite do carregador do Neutro X para efeitos


def be32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def sha256(data):
    return hashlib.sha256(data).hexdigest()


# --------------------------------------------------------------------------- XEX
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


# ------------------------------------------------------------------ XACT (XWB/XSB)
class BankError(Exception):
    pass


def parse_xwb(d):
    """Banco de ondas XACT3 (WBND/DNBW). Retorna dict com nome e lista de ondas."""
    if d[:4] == b"DNBW":
        e = ">"
    elif d[:4] == b"WBND":
        e = "<"
    else:
        raise BankError("assinatura WBND ausente")
    ver, hver = struct.unpack_from(e + "II", d, 4)
    if ver < 42 or ver > 46:
        raise BankError(f"versao XACT {ver} nao suportada (esperado 42-46, XACT3)")
    segs = [struct.unpack_from(e + "II", d, 0x0C + i * 8) for i in range(5)]
    bank_off = segs[0][0]
    flags, count = struct.unpack_from(e + "II", d, bank_off)
    name = d[bank_off + 8:bank_off + 72].split(b"\0")[0].decode("latin-1")
    meta_size = struct.unpack_from(e + "I", d, bank_off + 72)[0]
    if flags & 0x00020000:
        raise BankError("banco em formato compacto (nao suportado)")
    if meta_size < 24:
        raise BankError(f"metadados de {meta_size} bytes (nao suportado)")
    meta_off, data_off = segs[1][0], segs[4][0]
    waves = []
    for i in range(count):
        fd, fmt, po, pl = struct.unpack_from(e + "IIII", d, meta_off + i * meta_size)
        tag, ch = fmt & 3, (fmt >> 2) & 7
        rate, align, bits = (fmt >> 5) & 0x3FFFF, (fmt >> 23) & 0xFF, fmt >> 31
        data = d[data_off + po:data_off + po + pl]
        waves.append({"indice": i, "codec": ["PCM", "XMA2", "ADPCM", "WMA"][tag], "canais": ch,
                      "taxa_declarada": rate, "alinhamento": align, "bits": 16 if bits else 8,
                      "amostras_declaradas": fd >> 4, "bytes": len(data), "dados": data,
                      "big_endian": e == ">"})
    return {"nome": name, "versao": ver, "ondas": waves}


def parse_xsb(d):
    """Banco de sons XACT3 (SDBK/KBDS): cue -> lista de ondas. Somente cues simples."""
    if d[:4] == b"KBDS":
        e = ">"
    elif d[:4] == b"SDBK":
        e = "<"
    else:
        raise BankError("assinatura SDBK ausente")
    u16 = lambda o: struct.unpack_from(e + "H", d, o)[0]
    u32 = lambda o: struct.unpack_from(e + "I", d, o)[0]
    ver = u16(4)
    if ver < 42 or ver > 46:
        raise BankError(f"versao XACT {ver} nao suportada")
    simple_n, complex_n = u16(0x13), u16(0x15)
    simple_off, names_tbl = u32(0x22), u32(0x42)
    bank_name_off = u32(0x3A)
    wb_name = d[bank_name_off:bank_name_off + 64].split(b"\0")[0].decode("latin-1")
    cues, problems = [], []
    if complex_n:
        problems.append(f"{complex_n} cues complexas ignoradas (nao suportadas)")
    for i in range(simple_n):
        so = u32(simple_off + i * 5 + 1)
        no = u32(names_tbl + i * 6)
        name = d[no:d.index(b"\0", no)].decode("latin-1")
        try:
            if not d[so] & 1:  # som simples: indice da onda logo apos o cabecalho
                cues.append((name, [u16(so + 9)], "som simples"))
                continue
            clip = u32(so + 11)
            if d[so + 9] != 1 or d[clip] != 1:
                problems.append(f"cue {name}: varias trilhas/eventos (nao suportado)")
                continue
            ev = clip + 1
            typ = u32(ev) & 0x1F
            if typ in (1, 4):
                cues.append((name, [u16(ev + 8)], f"evento {typ}"))
            elif typ == 6:
                n = u16(ev + 22)
                lst = [u16(ev + 28 + k * 5) for k in range(n)]
                cues.append((name, lst, "evento 6 (variacao entre ondas)"))
            else:
                problems.append(f"cue {name}: evento tipo {typ} nao suportado")
        except (struct.error, ValueError, IndexError):
            problems.append(f"cue {name}: estrutura truncada")
    return {"banco_de_ondas": wb_name, "cues": cues, "problemas": problems}


def riff_for_xwb_wave(w, rate):
    """Envolve os dados de uma onda do banco em um RIFF que o FFmpeg decodifica."""
    data, ch = w["dados"], w["canais"]
    if w["codec"] == "XMA2":
        streams = (ch + 1) // 2
        fmt = struct.pack("<HHIIHHH", 0x0166, ch, rate, rate * ch * 2, 2048 * streams, 16, 34)
        fmt += struct.pack("<HIIIIIIIBBH", streams, 0x4 if ch == 1 else 0x3, w["amostras_declaradas"], len(data),
                           0, w["amostras_declaradas"], 0, 0, 0, 3, 1)
    elif w["codec"] == "PCM":
        bits = w["bits"]
        if bits == 16 and w["big_endian"]:
            data = b"".join(data[i + 1:i + 2] + data[i:i + 1] for i in range(0, len(data) - 1, 2))
        fmt = struct.pack("<HHIIHH", 1, ch, rate, rate * ch * bits // 8, ch * bits // 8, bits)
    else:
        return None
    body = b"WAVE" + b"fmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", len(data)) + data
    return b"RIFF" + struct.pack("<I", len(body)) + body


# --------------------------------------------------------------------- utilidades
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
            if r.returncode == 0 and "xma1" in r.stdout and "xma2" in r.stdout:
                return c
        except (OSError, subprocess.SubprocessError):
            continue
    return None


def riff_codec(data):
    """Codec e parametros declarados no fmt de um RIFF."""
    p = 12
    while p + 8 <= len(data):
        cid, sz = data[p:p + 4], struct.unpack_from("<I", data, p + 4)[0]
        if cid == b"fmt " and sz >= 16:
            tag, ch, rate = struct.unpack_from("<HHI", data, p + 8)
            if tag == 0x0165 and sz >= 32:  # XMAWAVEFORMAT: taxa/canais no 1o fluxo
                rate = struct.unpack_from("<I", data, p + 8 + 16 + 8)[0]
                ch = data[p + 8 + 16 + 17]
            return {0x0001: "PCM", 0x0003: "PCM float", 0x0165: "XMA", 0x0166: "XMA2",
                    0x0002: "ADPCM", 0x0161: "WMA"}.get(tag, f"0x{tag:04X}"), ch, rate
        p += 8 + sz + (sz & 1)
    return "desconhecido", 0, 0


def convert_and_validate(ffmpeg, src, dst, expect_samples=None):
    """Converte para WAV PCM 16 e valida. Retorna (ok, info|motivo)."""
    r = subprocess.run([ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-i", str(src), "-c:a", "pcm_s16le",
                        str(dst)], capture_output=True, text=True)
    if r.returncode != 0 or not dst.exists():
        return False, "FFmpeg falhou: " + (r.stderr.strip()[:120] or "sem mensagem")
    try:
        with wave.open(str(dst)) as w:
            ch, rate, n = w.getnchannels(), w.getframerate(), w.getnframes()
            pcm = w.readframes(n)
    except (wave.Error, EOFError) as ex:
        dst.unlink(missing_ok=True)
        return False, f"WAV gerado ilegivel ({ex})"
    problem = None
    if n == 0 or not pcm:
        problem = "WAV vazio"
    elif not 1 <= ch <= 2:
        problem = f"{ch} canais (esperado 1 ou 2)"
    elif n / rate > MAX_SECONDS:
        problem = f"duracao {n / rate:.1f} s acima do limite de {MAX_SECONDS:.0f} s para efeitos"
    else:
        peak = max(abs(v) for v in struct.unpack(f"<{len(pcm) // 2}h", pcm)) if pcm else 0
        if peak < 8:
            problem = "audio silencioso (pico abaixo de -72 dBFS)"
        elif expect_samples and abs(n - expect_samples) > 1024:
            problem = f"duracao divergente: {n} amostras, banco declara {expect_samples}"
    if problem:
        dst.unlink(missing_ok=True)
        return False, problem
    return True, {"canais": ch, "taxa": rate, "amostras": n, "segundos": round(n / rate, 3), "pico": peak,
                  "sha256": sha256(dst.read_bytes())}


def xur_sound_refs(name, data):
    """Strings de som referenciadas por uma cena/skin XUI (texto claro na tabela de strings)."""
    refs = set()
    for m in re.finditer(rb"[\x20-\x7e]{3,}", data):
        s = m.group().decode()
        low = s.lower()
        if "://" in low and low.endswith((".xma", ".wav", ".xwb", ".xsb")):
            refs.add(s)
        elif re.fullmatch(r"[a-z0-9_]+", low) and (low in RULE_INDEX or low in KNOWN_OTHER):
            refs.add(s)
    return refs


def clean_generated(folder):
    """Remove resultados de uma execucao anterior (somente pastas geradas por esta ferramenta)."""
    marker = [folder / n for n in ("manifesto.ini", "mapeamento.ini")]
    if not any(m.exists() and m.read_text(encoding="utf-8", errors="replace").startswith(GENERATED_TAG)
               for m in marker):
        return
    for p in folder.iterdir():
        if p.is_file() and p.suffix.lower() in (".wav", ".ini", ".txt"):
            p.unlink()


# ------------------------------------------------------------------- por versao
def process_version(x, info, dest, unpack, ffmpeg):
    folder, label = THEMES[info["build"]]
    tdir = dest / folder
    tdir.mkdir(parents=True, exist_ok=True)
    clean_generated(tdir)
    raw_dir = dest / "brutos" / folder
    if raw_dir.exists():
        shutil.rmtree(raw_dir)
    rep = []          # relatorio tecnico desta versao
    files = []        # audios instalados (manifesto)
    errors = []       # erros de conversao/validacao
    external = []     # recursos externos referenciados
    cands = {}        # efeito -> lista de (prioridade_fonte, rank, arquivo, evidencia)
    causes = {}
    hdr = xex_inspect.inspect(str(x))

    rep += [f"NEUTRO X - RELATORIO TECNICO DE AUDIO: {label} ({info['version']})",
            f"Arquivo: {x.name}  ({x.stat().st_size} bytes, SHA-256 {sha256(x.read_bytes())})", ""]
    rep.append("1. Cabecalho XEX2 (texto claro, lido sem descriptografar)")
    ex = hdr.get("execution_id", {})
    rep.append(f"   versao (Execution ID): {ex.get('version')}  - confere com {label}: "
               f"{'sim' if ex.get('version') == info['version'] else 'NAO'}")
    rep.append(f"   title id: {ex.get('title_id')}   nome PE original: {hdr.get('original_pe_name')}   "
               f"data do PE: {hdr.get('pe_timestamp')}")
    ff = hdr.get("file_format", {})
    rep.append(f"   criptografia: {ff.get('encryption')}   compressao: {ff.get('compression')}")
    libs = hdr.get("static_libraries", [])
    alibs = [lib for lib in libs if lib.split()[0].upper().startswith(AUDIO_LIBS)]
    rep.append("   bibliotecas de audio ligadas estaticamente: " + (", ".join(alibs) or "nenhuma listada"))
    rep.append("   bibliotecas importadas: " + "; ".join(hdr.get("import_libraries", [])))
    res = hdr.get("resources", [])
    rep.append(f"   pacotes de recursos ({len(res)}): " + ", ".join(r.split(" @")[0] for r in res))
    rep.append("")

    if info["encrypted"]:
        cause = ("XEX criptografado (AES): os pacotes de recursos - inclusive 'dashcomm', onde o Metro V2 guarda o "
                 "banco de sons dash.xwb/dash.xsb - so podem ser lidos com as chaves do console; nao contornado")
        has_dashcomm = any(r.startswith("dashcomm ") for r in res)
        rep.append("2. Conteudo: NAO ANALISADO")
        rep.append("   " + cause)
        if has_dashcomm:
            size = next(r for r in res if r.startswith("dashcomm ")).split("(")[-1].rstrip(")")
            rep.append(f"   o pacote 'dashcomm' existe nesta versao ({size}); e o candidato provavel aos sons, mas "
                       "isso nao pode ser verificado sem abrir o conteudo")
        rep.append("   para analisar esta versao e preciso uma copia NAO criptografada do mesmo XEX, obtida de forma "
                   "legitima pelo proprio usuario (a ferramenta processa qualquer XEX com corpo em texto claro)")
        for e in EFFECTS:
            causes[e] = cause
        status = "criptografado"
    else:
        status = "aberto"
        with tempfile.TemporaryDirectory() as tmp:
            r = subprocess.run([unpack, str(x), tmp], capture_output=True, text=True)
            if r.returncode != 0:
                cause = "falha ao extrair a imagem do XEX: " + (r.stderr.strip() or r.stdout.strip())[:120]
                rep.append("2. Conteudo: " + cause)
                for e in EFFECTS:
                    causes[e] = cause
                status = "falha"
            else:
                raw_dir.mkdir(parents=True, exist_ok=True)
                image = pathlib.Path(tmp, "imagem.bin").read_bytes()
                rep.append(f"2. Imagem descomprimida: {len(image)} bytes (LZX, sem criptografia)")
                api = sorted({m.group().decode() for m in re.finditer(
                    rb"[\x20-\x7e]*(?:[Xx][Aa]udio|XACT|[Xx]ui[Ss]ound|\.xwb|\.xsb)[\x20-\x7e]*", image)})
                rep.append("   referencias a audio no codigo (strings): " + ("; ".join(api[:12]) or "nenhuma"))
                pkgs = {}
                for resf in sorted(pathlib.Path(tmp, "recursos").glob("*.bin")):
                    pkgs[resf.stem] = parse_xuiz(resf.read_bytes())
                banks_xwb, banks_xsb, riffs, xur_refs = {}, {}, [], {}
                for pkg, entries in pkgs.items():
                    for name, data in entries:
                        low = name.lower()
                        if low.endswith(".xwb"):
                            banks_xwb[f"{pkg}/{name}"] = data
                        elif low.endswith(".xsb"):
                            banks_xsb[f"{pkg}/{name}"] = data
                        elif low.endswith(AUDIO_EXT):
                            riffs.append((pkg, name, data))
                        elif low.endswith(".xur"):
                            for ref in xur_sound_refs(name, data):
                                xur_refs.setdefault(ref.lower(), set()).add(f"{pkg}/{name}")
                # bancos soltos ao lado do XEX
                for ext in sorted(set(x.parent.glob("*.xwb")) | set(x.parent.glob("*.xsb"))):
                    (banks_xwb if ext.suffix.lower() == ".xwb" else banks_xsb)[f"(arquivo externo)/{ext.name}"] = \
                        ext.read_bytes()
                rep.append(f"   pacotes XUIZ lidos: {len(pkgs)}; bancos XACT: {len(banks_xwb)} XWB, "
                           f"{len(banks_xsb)} XSB; audios RIFF soltos: {len(riffs)}")
                rep.append("")
                rep.append("3. Bancos XACT")
                for key, data in sorted(banks_xwb.items()) + sorted(banks_xsb.items()):
                    (raw_dir / key.replace("/", "__").replace("\\", "_")).write_bytes(data)
                xsb_parsed = {}
                for key, data in sorted(banks_xsb.items()):
                    try:
                        xs = parse_xsb(data)
                        xsb_parsed[key] = xs
                        rep.append(f"   {key}: {len(xs['cues'])} cues, banco de ondas '{xs['banco_de_ondas']}'")
                        for p in xs["problemas"]:
                            rep.append(f"      aviso: {p}")
                    except (BankError, struct.error, ValueError) as ex:
                        rep.append(f"   {key}: XSB nao interpretado ({ex})")
                for key, data in sorted(banks_xwb.items()):
                    try:
                        wb = parse_xwb(data)
                    except (BankError, struct.error) as ex:
                        rep.append(f"   {key}: XWB nao interpretado ({ex})")
                        errors.append(f"{key}: {ex}")
                        continue
                    codecs = sorted({w["codec"] for w in wb["ondas"]})
                    rep.append(f"   {key}: banco '{wb['nome']}' XACT v{wb['versao']}, {len(wb['ondas'])} ondas, "
                               f"codec {', '.join(codecs)}")
                    # cues que apontam para este banco
                    by_wave = {}
                    for xs in xsb_parsed.values():
                        if xs["banco_de_ondas"] != wb["nome"]:
                            continue
                        for cue, waves, kind in xs["cues"]:
                            for k, wi in enumerate(waves):
                                alias = cue if len(waves) == 1 else f"{cue} (variacao {k + 1}/{len(waves)})"
                                by_wave.setdefault(wi, []).append((cue, alias, kind))
                    stem_bank = pathlib.PurePosixPath(key.replace("\\", "/")).stem
                    for w in wb["ondas"]:
                        cues = by_wave.get(w["indice"], [])
                        # nome do arquivo: a cue de maior prioridade (regras), senao a primeira
                        ranked = sorted(cues, key=lambda c: RULE_INDEX.get(c[0].lower(), (None, None, 99))[2])
                        primary = ranked[0][0] if ranked else f"{stem_bank}_onda{w['indice']:02}"
                        if (tdir / f"{primary}.wav").exists() or ranked and ranked[0][1] != ranked[0][0]:
                            primary = f"{primary}_onda{w['indice']:02}"  # variacoes da mesma cue: um arquivo cada
                        rate = min(XMA_RATES, key=lambda r: abs(r - w["taxa_declarada"])) \
                            if w["codec"] == "XMA2" else w["taxa_declarada"]
                        wrapped = riff_for_xwb_wave(w, rate)
                        src_desc = f"{key} onda {w['indice']}" + (f", cue(s) {', '.join(c[1] for c in cues)}"
                                                                    if cues else ", sem cue")
                        if wrapped is None:
                            errors.append(f"{src_desc}: codec {w['codec']} nao suportado")
                            rep.append(f"      [erro] onda {w['indice']}: codec {w['codec']} nao suportado")
                            continue
                        raw = raw_dir / f"{stem_bank}_onda{w['indice']:02}.{w['codec'].lower()}.riff"
                        raw.write_bytes(wrapped)
                        wav = tdir / f"{primary}.wav"
                        ok, res_v = convert_and_validate(ffmpeg, raw, wav, w["amostras_declaradas"])
                        if not ok:
                            errors.append(f"{src_desc}: {res_v}")
                            rep.append(f"      [erro] onda {w['indice']} ({primary}): {res_v}")
                            continue
                        rate_note = ""
                        if w["codec"] == "XMA2" and w["taxa_declarada"] != rate:
                            rate_note = f"; taxa no banco {w['taxa_declarada']} -> XMA a {rate} Hz"
                        files.append(classify_file(wav.name, [c[0] for c in cues], src_desc,
                                                   f"{w['codec']} {w['canais']}ch", res_v, xur_refs, cands,
                                                   source_rank=0, uncertain_rate=abs(w["taxa_declarada"] - rate)
                                                   > rate * 0.01 and w["codec"] == "XMA2"))
                        rep.append(f"      onda {w['indice']:2} -> {wav.name}: {files[-1]['status']}"
                                   f"{' => ' + files[-1]['efeito'] if files[-1]['efeito'] else ''}; "
                                   f"{res_v['segundos']} s, {res_v['canais']}ch {res_v['taxa']} Hz{rate_note}; "
                                   f"cues: {', '.join(c[1] for c in cues) or 'nenhuma'}")
                rep.append("")
                rep.append("4. Audios RIFF soltos nos pacotes")
                for pkg, name, data in riffs:
                    stem = pathlib.PurePosixPath(name.replace("\\", "/")).stem
                    if data[:4] != b"RIFF":
                        rep.append(f"   [referencia] {pkg}/{name}: sem dados de audio no pacote")
                        continue
                    codec, ch, rate = riff_codec(data)
                    raw = raw_dir / f"{pkg}__{stem}{pathlib.PurePosixPath(name).suffix.lower()}"
                    raw.write_bytes(data)
                    wav = tdir / f"{pkg}__{stem}.wav"
                    ok, res_v = convert_and_validate(ffmpeg, raw, wav)
                    if not ok:
                        errors.append(f"{pkg}/{name}: {res_v}")
                        rep.append(f"   [erro] {pkg}/{name}: {res_v}")
                        continue
                    files.append(classify_file(wav.name, [stem], f"{pkg}/{name}", f"{codec} {ch}ch {rate} Hz",
                                               res_v, xur_refs, cands, source_rank=1))
                    rep.append(f"   {pkg}/{name} ({codec}) -> {wav.name}: {files[-1]['status']}"
                               f"{' => ' + files[-1]['efeito'] if files[-1]['efeito'] else ''}; {res_v['segundos']} s")
                if not riffs:
                    rep.append("   nenhum")
                rep.append("")
                rep.append("5. Referencias a sons em cenas/skins XUI (.xur)")
                for ref in sorted(xur_refs):
                    where = ", ".join(sorted(xur_refs[ref])[:3])
                    rep.append(f"   {ref}  <- {where}")
                    if ref.startswith("sharedres://"):
                        external.append(f"{ref} (recurso compartilhado do sistema/XAM, fora deste XEX; "
                                        f"usado por {where})")
                if external:
                    rep.append("   recursos EXTERNOS (nao contidos neste XEX):")
                    rep += [f"      {e}" for e in external]
                for e in EFFECTS:
                    if e not in cands:
                        causes[e] = ("nenhuma cue/arquivo com nome correspondente a esta funcao entre os sons do "
                                     "XEX" + (" (bancos XACT e audios soltos verificados)" if banks_xwb else ""))
    # escolha final por efeito: fonte preferida (banco XACT) e regra de maior prioridade
    found = {}
    for e, lst in cands.items():
        lst.sort()
        found[e] = lst[0]
        for f in files:
            if f["arquivo"] == lst[0][2]:
                f["usado_para"].append(e)
    for f in files:
        if f["status"] == "confirmado" and not f["usado_para"]:
            f["status"] = "alternativa"
    unconfirmed = sum(f["status"] == "nao_confirmado" for f in files)
    situ = ("sem acesso: XEX criptografado" if status == "criptografado" else
            "falha na extracao" if status == "falha" else
            f"{'completo' if len(found) == len(EFFECTS) else 'parcial'}: {len(found)} de {len(EFFECTS)} efeitos "
            f"confirmados")
    rep.append("")
    rep.append("6. Resultado para o Neutro X")
    for e in EFFECTS:
        if e in found:
            rep.append(f"   {e:10} <- {found[e][2]}  ({found[e][3]})")
        else:
            rep.append(f"   {e:10} AUSENTE: {causes.get(e, 'sem evidencia')}")
    rep.append(f"   audios instalados: {len(files)}; nao confirmados: {unconfirmed}; erros de conversao: "
               f"{len(errors)}; situacao: {situ}")
    for er in errors:
        rep.append(f"   erro: {er}")

    # ---- arquivos de saida da versao
    (tdir / "mapeamento.ini").write_text(
        GENERATED_TAG + " - somente funcoes CONFIRMADAS por evidencia\n" +
        "".join(f"{e}={found[e][2]}\n" for e in EFFECTS if e in found), encoding="utf-8")
    (tdir / "ausentes.ini").write_text(
        GENERATED_TAG + " - efeitos sem audio original e o motivo\n" +
        "".join(f"{e}={causes.get(e, 'sem evidencia')}\n" for e in EFFECTS if e not in found), encoding="utf-8")
    man = [GENERATED_TAG, f"dashboard={label}", f"versao={info['version']}", f"xex={x.name}", f"xex_estado={status}",
           f"audios_originais={len(files)}", f"efeitos_esperados={len(EFFECTS)}", f"efeitos_confirmados={len(found)}",
           f"nao_confirmados={unconfirmed}", f"erros_conversao={len(errors)}", f"recursos_externos={len(external)}",
           f"situacao={situ}",
           "# arquivo.<nome>=status|efeito|origem|codec|canais|taxa|segundos|sha256"]
    for f in files:
        v = f["validacao"]
        man.append(f"arquivo.{f['arquivo']}={f['status']}|{','.join(f['usado_para']) or f['efeito'] or '-'}|"
                   f"{f['origem']}|{f['codec']}|{v['canais']}|{v['taxa']}|{v['segundos']}|{v['sha256']}")
    (tdir / "manifesto.ini").write_text("\n".join(man) + "\n", encoding="utf-8")
    (tdir / "relatorio.txt").write_text("\n".join(rep) + "\n", encoding="utf-8")
    return {"dashboard": label, "versao": info["version"], "pasta": folder, "xex": x.name, "xex_estado": status,
            "situacao": situ, "efeitos": {e: (found[e][2] if e in found else None) for e in EFFECTS},
            "ausentes": {e: causes.get(e) for e in EFFECTS if e not in found}, "arquivos": files,
            "nao_confirmados": unconfirmed, "erros_conversao": errors, "recursos_externos": external}, rep


def classify_file(fname, names, origin, codec, val, xur_refs, cands, source_rank, uncertain_rate=False):
    """Classifica um audio pelo(s) nome(s) originais. Registra candidato a efeito quando confirmado."""
    entry = {"arquivo": fname, "nomes_originais": names, "origem": origin, "codec": codec, "validacao": val,
             "status": "nao_confirmado", "efeito": None, "funcao_conhecida": None, "evidencia": None,
             "usado_para": []}
    if uncertain_rate:
        entry["evidencia"] = ("taxa declarada no banco foge mais de 1% das taxas do XMA: decodificacao incerta, "
                              "nao usado")
        return entry
    best = None
    for n in names:
        low = n.lower()
        if low in RULE_INDEX and (best is None or RULE_INDEX[low][2] < best[2]):
            best = (*RULE_INDEX[low], n)
    if best:
        eff, why, rank, n = best
        ev = f"nome original '{n}' = {why}"
        refs = set(xur_refs.get(n.lower(), set()))
        shared = [k for k in xur_refs if k.endswith(f"://{n.lower()}.xma")]
        for k in shared:
            refs |= xur_refs[k]
        if refs:
            ev += "; usado por " + ", ".join(sorted(refs)[:2])
        if shared:
            ev += f" (a skin tambem nomeia {shared[0]})"
        entry.update(status="confirmado", efeito=eff, evidencia=ev)
        cands.setdefault(eff, []).append((source_rank, rank, fname, ev))
        return entry
    for n in names:
        if n.lower() in KNOWN_OTHER:
            entry.update(status="sem_evento", funcao_conhecida=KNOWN_OTHER[n.lower()],
                         evidencia=f"nome original '{n}': {KNOWN_OTHER[n.lower()]} (o Neutro X nao tem esse evento)")
            return entry
    entry["evidencia"] = "sem cue/nome que indique a funcao" if not names else \
        f"nome(s) {', '.join(names)} sem funcao verificavel"
    return entry


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--entrada", required=True)
    ap.add_argument("--destino", default=os.path.join(os.environ.get("LOCALAPPDATA", "."), "NeutroX", "local_audio"))
    ap.add_argument("--unpack", default=None)
    ap.add_argument("--ffmpeg", default=None)
    a = ap.parse_args(argv)

    here = pathlib.Path(__file__).resolve().parent.parent
    unpack = a.unpack or next((str(p) for p in [here / "build_release/Release/neutrox_xex_unpack.exe",
                                                 here / "build/Release/neutrox_xex_unpack.exe",
                                                 here / "build/neutrox_xex_unpack"] if p.exists()), None)
    if not unpack:
        print("ERRO: neutrox_xex_unpack nao encontrado (compile o projeto ou use --unpack)")
        return 2
    a.ffmpeg = find_ffmpeg(a.ffmpeg)
    if not a.ffmpeg:
        print("ERRO: nenhum ffmpeg com decodificadores XMA/XMA2 encontrado (instale o FFmpeg completo ou use --ffmpeg)")
        return 2
    src = pathlib.Path(a.entrada)
    dest = pathlib.Path(a.destino)
    dest.mkdir(parents=True, exist_ok=True)
    summary = [f"Neutro X - extracao de sons originais ({datetime.datetime.now():%d/%m/%Y %H:%M})",
               f"Entrada: {src}", f"Destino: {dest}", f"Conversor: {a.ffmpeg}", ""]
    results = []
    for x in sorted(src.rglob("*.xex")):
        info = xex_info(x)
        if not info or info["build"] not in THEMES:
            summary.append(f"[ignorado] {x.name}: nao e um dashboard conhecido")
            continue
        res, rep = process_version(x, info, dest, unpack, a.ffmpeg)
        results.append(res)
        summary += rep + ["", "=" * 78, ""]
    seen = {r["pasta"] for r in results}
    for b, (folder, label) in THEMES.items():
        if folder not in seen:
            summary.append(f"== {label}: nenhum XEX desta versao na entrada")
    summary.append("RESUMO")
    for r in results:
        n = sum(v is not None for v in r["efeitos"].values())
        summary.append(f"  {r['dashboard']:9} {r['versao']:13} {r['situacao']}; audios: {len(r['arquivos'])}, "
                       f"nao confirmados: {r['nao_confirmados']}, erros: {len(r['erros_conversao'])}")
    text = "\n".join(summary) + "\n"
    (dest / "relatorio_extracao.txt").write_text(text, encoding="utf-8")
    (dest / "manifesto.json").write_text(json.dumps({"gerado_em": datetime.datetime.now().isoformat(timespec="seconds"),
                                                     "efeitos": EFFECTS, "versoes": results},
                                                    ensure_ascii=False, indent=1), encoding="utf-8")
    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
