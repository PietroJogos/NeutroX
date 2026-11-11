#!/usr/bin/env python3
"""
xex_inspect.py - Leitor somente-leitura de cabecalhos XEX2 (Xbox 360).

NAO executa, NAO descriptografa e NAO extrai codigo ou recursos do arquivo.
Le apenas o cabecalho publico (big-endian) para relatorio:
  - magic, flags do modulo, offsets
  - Execution ID (versao, base version, title id, media id)
  - nome PE original, ponto de entrada, endereco base, data/hora
  - formato do arquivo (tipo de criptografia/compressao)
  - bibliotecas estaticas (versao do kit de compilacao usado)
  - bibliotecas importadas (nomes e versoes)

Uso:  python -I xex_inspect.py arquivo.xex [arquivo2.xex ...] [--json saida.json]
"""
import datetime
import json
import struct
import sys

HEADER_NAMES = {
    0x000002FF: "RESOURCE_INFO",
    0x000003FF: "FILE_FORMAT_INFO",
    0x00000405: "BASE_REFERENCE",
    0x000005FF: "DELTA_PATCH_DESCRIPTOR",
    0x000080FF: "BOUNDING_PATH",
    0x00008105: "DEVICE_ID",
    0x00010001: "ORIGINAL_BASE_ADDRESS",
    0x00010100: "ENTRY_POINT",
    0x00010201: "IMAGE_BASE_ADDRESS",
    0x000103FF: "IMPORT_LIBRARIES",
    0x00018002: "CHECKSUM_TIMESTAMP",
    0x00018102: "ENABLED_FOR_CALLCAP",
    0x00018200: "ENABLED_FOR_FASTCAP",
    0x000183FF: "ORIGINAL_PE_NAME",
    0x000200FF: "STATIC_LIBRARIES",
    0x00020104: "TLS_INFO",
    0x00020200: "DEFAULT_STACK_SIZE",
    0x00020301: "DEFAULT_FILESYSTEM_CACHE_SIZE",
    0x00020401: "DEFAULT_HEAP_SIZE",
    0x00028002: "PAGE_HEAP_SIZE_AND_FLAGS",
    0x00030000: "SYSTEM_FLAGS",
    0x00040006: "EXECUTION_INFO",
    0x00040201: "TITLE_WORKSPACE_SIZE",
    0x00040310: "GAME_RATINGS",
    0x00040404: "LAN_KEY",
    0x000405FF: "XBOX360_LOGO",
    0x000406FF: "MULTIDISC_MEDIA_IDS",
    0x000407FF: "ALTERNATE_TITLE_IDS",
    0x00040801: "ADDITIONAL_TITLE_MEMORY",
    0x00E10402: "EXPORTS_BY_NAME",
}

ENCRYPTION = {0: "nenhuma", 1: "normal (AES)"}
COMPRESSION = {0: "nenhuma", 1: "basica (sem compressao, blocos)", 2: "normal (LZX)", 3: "delta"}


def u16(b, o):
    return struct.unpack_from(">H", b, o)[0]


def u32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def fmt_version(v):
    """Versao empacotada XEX: major:4 minor:4 build:16 qfe:8 (total 32 bits)."""
    major = (v >> 28) & 0xF
    minor = (v >> 24) & 0xF
    build = (v >> 8) & 0xFFFF
    qfe = v & 0xFF
    return f"{major}.{minor}.{build}.{qfe}"


def cstr(b, o, maxlen=256):
    end = b.find(b"\0", o, o + maxlen)
    if end < 0:
        end = o + maxlen
    return b[o:end].decode("ascii", errors="replace")


def inspect(path):
    with open(path, "rb") as f:
        # So o cabecalho e necessario; le o suficiente (cabecalhos XEX sao pequenos).
        head = f.read(0x10000)
        f.seek(0, 2)
        file_size = f.tell()

    r = {"file": path, "size_bytes": file_size, "errors": []}
    if len(head) < 0x18 or head[0:4] != b"XEX2":
        r["errors"].append(f"magic invalido: {head[0:4]!r} (esperado b'XEX2')")
        return r

    r["magic"] = "XEX2"
    module_flags = u32(head, 4)
    pe_offset = u32(head, 8)
    sec_offset = u32(head, 0x10)
    count = u32(head, 0x14)
    r["module_flags"] = f"0x{module_flags:08X}"
    flag_names = []
    for bit, name in [(0, "TITLE"), (1, "EXPORTS_TO_TITLE"), (2, "SYSTEM_DEBUGGER"), (3, "DLL"),
                      (4, "MODULE_PATCH"), (5, "PATCH_FULL"), (6, "PATCH_DELTA"), (7, "USER_MODE")]:
        if module_flags & (1 << bit):
            flag_names.append(name)
    r["module_flag_names"] = flag_names
    r["pe_data_offset"] = f"0x{pe_offset:08X}"
    r["security_info_offset"] = f"0x{sec_offset:08X}"
    r["optional_header_count"] = count

    if pe_offset > file_size:
        r["errors"].append("offset de dados PE alem do fim do arquivo (arquivo truncado?)")

    headers = []
    for i in range(count):
        o = 0x18 + i * 8
        if o + 8 > len(head):
            r["errors"].append("tabela de cabecalhos opcionais truncada")
            break
        key, val = u32(head, o), u32(head, o + 4)
        name = HEADER_NAMES.get(key, f"DESCONHECIDO_{key:08X}")
        headers.append((key, val, name))
    r["optional_headers"] = [f"{n} (0x{k:08X})" for k, v, n in headers]

    for key, val, name in headers:
        try:
            if name == "EXECUTION_INFO":
                o = val
                media_id, ver, base_ver, title_id = struct.unpack_from(">IIII", head, o)
                platform, exe_type, disc_no, disc_cnt = struct.unpack_from(">BBBB", head, o + 16)
                r["execution_id"] = {
                    "media_id": f"0x{media_id:08X}",
                    "version": fmt_version(ver),
                    "base_version": fmt_version(base_ver),
                    "title_id": f"0x{title_id:08X}",
                    "platform": platform,
                    "executable_type": exe_type,
                    "disc": f"{disc_no}/{disc_cnt}",
                }
            elif name == "RESOURCE_INFO":
                # Tabela de recursos embutidos (fica no cabecalho, que nao e criptografado):
                # u32 tamanho; entradas de 16 bytes: nome[8], endereco u32, tamanho u32
                size = u32(head, val)
                res = []
                for i in range((size - 4) // 16):
                    o = val + 4 + i * 16
                    rname = head[o:o + 8].rstrip(b"\0").decode("ascii", "replace")
                    addr, rsize = struct.unpack_from(">II", head, o + 8)
                    res.append(f"{rname} @0x{addr:08X} ({rsize} bytes)")
                r["resources"] = res
            elif name == "ORIGINAL_PE_NAME":
                size = u32(head, val)
                r["original_pe_name"] = cstr(head, val + 4, size - 4)
            elif name == "ENTRY_POINT":
                r["entry_point"] = f"0x{val:08X}"
            elif name == "IMAGE_BASE_ADDRESS":
                r["image_base_address"] = f"0x{val:08X}"
            elif name == "DEFAULT_STACK_SIZE":
                r["default_stack_size"] = val
            elif name == "SYSTEM_FLAGS":
                r["system_flags"] = f"0x{val:08X}"
            elif name == "CHECKSUM_TIMESTAMP":
                checksum, ts = struct.unpack_from(">II", head, val)
                r["pe_checksum"] = f"0x{checksum:08X}"
                r["pe_timestamp"] = datetime.datetime.fromtimestamp(ts, datetime.timezone.utc).strftime("%Y-%m-%d %H:%M:%S UTC")
            elif name == "FILE_FORMAT_INFO":
                enc, comp = struct.unpack_from(">HH", head, val + 4)
                r["file_format"] = {"encryption": ENCRYPTION.get(enc, str(enc)),
                                    "compression": COMPRESSION.get(comp, str(comp))}
            elif name == "STATIC_LIBRARIES":
                size = u32(head, val)
                n = (size - 4) // 16
                libs = []
                for i in range(n):
                    o = val + 4 + i * 16
                    lname = head[o:o + 8].rstrip(b"\0").decode("ascii", "replace")
                    major, minor, build, flags = struct.unpack_from(">HHHH", head, o + 8)
                    qfe = flags & 0xFF
                    libs.append(f"{lname} {major}.{minor}.{build}.{qfe}")
                r["static_libraries"] = libs
            elif name == "IMPORT_LIBRARIES":
                size = u32(head, val)
                str_size, str_count = struct.unpack_from(">II", head, val + 4)
                names = []
                base = val + 12
                p = base
                end = base + str_size
                while p < end and len(names) < str_count:
                    s = cstr(head, p, end - p)
                    names.append(s)
                    p += len(s) + 1
                    # cada string e alinhada em 4 bytes; o preenchimento pode conter lixo
                    p = base + ((p - base + 3) & ~3)
                p = val + 12 + str_size
                imports = []
                stop = val + size
                while p < stop:
                    rec_size = u32(head, p)
                    if rec_size == 0:
                        break
                    # size(4) digest(20) import_id(4) version(4) version_min(4) name_index(2) count(2)
                    version = u32(head, p + 28)
                    version_min = u32(head, p + 32)
                    name_index = u16(head, p + 36)
                    rcount = u16(head, p + 38)
                    lib = names[name_index] if name_index < len(names) else f"#{name_index}"
                    imports.append(f"{lib} v{fmt_version(version)} (min {fmt_version(version_min)}, {rcount} registros)")
                    p += rec_size
                r["import_libraries"] = imports
        except (struct.error, IndexError) as e:
            r["errors"].append(f"falha ao ler {name}: {e}")

    # Informacoes de seguranca (somente campos nao sensiveis)
    try:
        o = sec_offset
        header_size, image_size = struct.unpack_from(">II", head, o)
        image_flags, load_addr = struct.unpack_from(">II", head, o + 8 + 256 + 4)
        # Apos load_address: section_digest(20) import_table_count(4) import_digest(20)
        # media_id(16) aes_key(16) export_table(4) header_digest(20) region(4) allowed_media(4)
        p = o + 8 + 256 + 4 + 8 + 20 + 4 + 20 + 16 + 16 + 4 + 20
        region, allowed_media = struct.unpack_from(">II", head, p)
        r["security_info"] = {
            "header_size": header_size,
            "image_size": image_size,
            "image_flags": f"0x{image_flags:08X}",
            "load_address": f"0x{load_addr:08X}",
            "game_regions": f"0x{region:08X}",
            "allowed_media_types": f"0x{allowed_media:08X}",
        }
    except struct.error as e:
        r["errors"].append(f"falha ao ler security info: {e}")

    # Varredura somente-leitura por assinaturas de audio em texto claro no arquivo
    # bruto. Nao descriptografa nem descomprime nada: em arquivos criptografados
    # ou comprimidos, a ausencia de assinaturas e esperada.
    sigs = {b"RIFF": "RIFF (WAV/XMA em RIFF)", b"WAVE": "WAVE", b"XWB": "XACT wave bank",
            b"WBND": "XACT wave bank (WBND)", b"SDBK": "XACT sound bank (SDBK)", b"XMA2": "XMA2",
            b"OggS": "Ogg", b"fLaC": "FLAC", b"ID3": "MP3/ID3", b".wav": "nome .wav", b".xma": "nome .xma",
            b".xwb": "nome .xwb", b".xsb": "nome .xsb", b".xzp": "pacote XZP", b".xur": "recurso XUI"}
    with open(path, "rb") as f:
        data = f.read()
    body = data[pe_offset:] if pe_offset < len(data) else b""
    found = {}
    for sig, label in sigs.items():
        c = body.count(sig)
        if c:
            found[label] = c
    r["audio_signature_scan"] = found if found else "nenhuma assinatura de audio em texto claro"
    # Entropia aproximada do corpo (perto de 8 bits/byte = criptografado/comprimido)
    import math
    sample = body[: 1 << 20]
    if sample:
        freq = [0] * 256
        for b in sample:
            freq[b] += 1
        ent = -sum((c / len(sample)) * math.log2(c / len(sample)) for c in freq if c)
        r["body_entropy_bits_per_byte"] = round(ent, 3)
    return r


def main(argv):
    out_json = None
    files = []
    i = 0
    while i < len(argv):
        if argv[i] == "--json":
            out_json = argv[i + 1]
            i += 2
            continue
        files.append(argv[i])
        i += 1
    results = [inspect(p) for p in files]
    for r in results:
        print("=" * 72)
        print(r["file"])
        for k, v in r.items():
            if k == "file":
                continue
            if isinstance(v, list):
                print(f"  {k}:")
                for item in v:
                    print(f"    - {item}")
            elif isinstance(v, dict):
                print(f"  {k}:")
                for kk, vv in v.items():
                    print(f"    {kk}: {vv}")
            else:
                print(f"  {k}: {v}")
    if out_json:
        with open(out_json, "w", encoding="utf-8") as f:
            json.dump(results, f, indent=2, ensure_ascii=False)
    return 0 if all(not r["errors"] for r in results) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
