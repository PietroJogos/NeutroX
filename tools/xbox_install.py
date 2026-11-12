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
  - opcional: --covers envia capas/ (capas ja baixadas no PC).

O que NAO faz: nao apaga nada no console, nao grava fora da pasta de destino, nao
toca em NAND, sistema, DashLaunch ou Aurora, e nao guarda a senha (pedida na hora,
ou --password para automatizar por sua conta).
"""
import argparse
import ftplib
import getpass
import os
import posixpath
import sys

VERSIONS = ["blades_6717", "nxe_9199", "kinect_13604", "metro_v1_15574", "metro_v2_17559"]
AUDIO_EXT = {".wav", ".ini", ".txt"}


def default_local_audio():
    base = os.environ.get("LOCALAPPDATA") or os.path.expanduser("~")
    return os.path.join(base, "NeutroX", "local_audio")


def default_xex():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    candidates = [os.path.join(root, "NeutroX.xex")]
    dist = os.path.join(root, "dist")
    if os.path.isdir(dist):
        for d in sorted(os.listdir(dist), reverse=True):
            candidates.append(os.path.join(dist, d, "NeutroX.xex"))
    for c in candidates:
        if os.path.isfile(c):
            return c
    return None


def plan(args):
    """Lista (local, remoto) a enviar."""
    items = []
    if args.xex:
        items.append((args.xex, posixpath.join(args.dest, "NeutroX.xex")))
    if not args.no_audio:
        la = args.local_audio
        if not os.path.isdir(la):
            print(f"AVISO: pasta de sons originais nao encontrada: {la} (rode tools/sound_extract.py antes)")
        else:
            for v in VERSIONS:
                vd = os.path.join(la, v)
                if not os.path.isdir(vd):
                    continue
                for name in sorted(os.listdir(vd)):
                    p = os.path.join(vd, name)
                    if os.path.isfile(p) and os.path.splitext(name)[1].lower() in AUDIO_EXT:
                        items.append((p, posixpath.join(args.dest, "local_audio", v, name)))
    if args.covers:
        cd = args.covers
        for name in sorted(os.listdir(cd)) if os.path.isdir(cd) else []:
            p = os.path.join(cd, name)
            if os.path.isfile(p) and os.path.splitext(name)[1].lower() in {".jpg", ".png", ".ini"}:
                items.append((p, posixpath.join(args.dest, "capas", name)))
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
    ap.add_argument("--host", required=True, help="IP do console (tela do Aurora)")
    ap.add_argument("--port", type=int, default=21)
    ap.add_argument("--user", default="xboxftp", help="usuario FTP do Aurora (padrao xboxftp)")
    ap.add_argument("--password", default=None, help="senha FTP (se omitida, e pedida)")
    ap.add_argument("--dest", default="/Hdd1/Apps/NeutroX", help="pasta do Neutro X no console")
    ap.add_argument("--xex", default=None, help="NeutroX.xex a enviar (padrao: o mais recente em dist/)")
    ap.add_argument("--local-audio", default=default_local_audio())
    ap.add_argument("--no-audio", action="store_true", help="nao enviar local_audio/")
    ap.add_argument("--covers", default=None, help="pasta de capas no PC para enviar a capas/")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()
    if args.xex is None:
        args.xex = default_xex()
        if args.xex is None:
            print("AVISO: NeutroX.xex nao encontrado; use --xex. Enviando apenas os sons.")
    args.dest = "/" + args.dest.strip("/")

    items = plan(args)
    if not items:
        print("Nada a enviar.")
        return 1
    total = sum(os.path.getsize(l) for l, _ in items)
    print(f"{len(items)} arquivos, {total / 1048576:.1f} MB -> ftp://{args.host}{args.dest}")
    if args.dry_run:
        for l, r in items:
            print(f"  {l} -> {r}")
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
        sent += 1
        print(f"  [{sent}/{len(items)}] {remote}")
    ftp.quit()
    print("Concluido. Abra o NeutroX.xex pelo Aurora; em Configuracoes > Sons originais: estado confira o que carregou.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
