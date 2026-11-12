"""Localiza chamadas de áudio em chunks Lua 5.1, sem executar bytecode."""
import struct


def audio_calls(data, sound_module=False):
    # Lua 5.1: int, size_t e Instruction de 32 bits, lua_Number double.
    if len(data) < 12 or data[:6] != b"\x1bLuaQ\x00" or data[6] not in (0, 1) or data[7:12] != b"\x04\x04\x04\x08\x00":
        return []
    endian = "<" if data[6] else ">"
    pos = 12
    matches = []

    def take(n):
        nonlocal pos
        if n > len(data) - pos:
            raise ValueError("chunk Lua truncado")
        view = data[pos:pos + n]
        pos += n
        return view

    def integer():
        return struct.unpack(endian + "I", take(4))[0]

    def string():
        size = integer()
        if not size:
            return ""
        return take(size)[:-1].decode("latin-1")

    def constant():
        tag = take(1)[0]
        if tag == 4:
            return string()
        if tag == 1:
            take(1)
        elif tag == 3:
            take(8)
        elif tag != 0:
            raise ValueError("constante Lua desconhecida")
        return ""

    def function(depth, name):
        if depth > 64:
            raise ValueError("chunk Lua muito aninhado")
        string()
        start, _ = integer(), integer()
        take(4)
        code = take(integer() * 4)
        size = integer()
        if size > len(data) - pos:
            raise ValueError("tabela de constantes Lua inválida")
        constants = [constant() for _ in range(size)]
        instructions = list(struct.iter_unpack(endian + "I", code))
        # Não propaga registradores por entradas de salto.
        targets = set()
        for pc, (word,) in enumerate(instructions):
            if word & 63 in (22, 31, 32):
                targets.add(pc + 1 + (word >> 14) - 131071)
        registers = {}

        def rk(index):
            if index & 256:
                k = index & 255
                return constants[k] if k < len(constants) else ""
            return registers.get(index, "")

        for pc, (word,) in enumerate(instructions):
            if pc in targets:
                registers.clear()
            op, a, b, c = word & 63, (word >> 6) & 255, word >> 23, (word >> 14) & 511
            bx = word >> 14
            if op == 0:  # MOVE
                registers[a] = registers.get(b, "")
            elif op in (1, 5):  # LOADK, GETGLOBAL
                registers[a] = constants[bx] if bx < len(constants) else ""
            elif op in (6, 11):  # GETTABLE, SELF
                parent, member = registers.get(b, ""), rk(c)
                registers[a] = ".".join(part for part in (parent, member) if part)
                if op == 11:
                    registers[a + 1] = parent
            elif op in (28, 29):  # CALL, TAILCALL
                symbol = registers.get(a, "")
                low = symbol.casefold()
                member = low.rsplit(".", 1)[-1]
                if ("playsound" in member or "playcue" in member or
                        (member in ("play", "stop") and (sound_module or "sound" in low))):
                    args = [registers[k] for k in range(a + 1, a + b) if registers.get(k)] if b else []
                    matches.append((name, start, pc, symbol, args))
                for k in tuple(registers):
                    if k >= a:
                        registers.pop(k)
            elif op in (22, 23, 24, 25, 26, 27, 30, 31, 32, 33) or (op == 2 and c):
                registers.clear()
            elif op == 3:  # LOADNIL (range)
                for k in range(a, b + 1):
                    registers.pop(k, None)
            elif op not in (7, 8, 9, 34, 35):  # operações que não escrevem A
                registers.pop(a, None)
        count = integer()
        if count > len(data) - pos:
            raise ValueError("tabela de funções Lua inválida")
        for child in range(count):
            function(depth + 1, f"{name}.{child}")
        take(integer() * 4)  # linhas
        count = integer()
        if count > len(data) - pos:
            raise ValueError("tabela de locais Lua inválida")
        for _ in range(count):
            string()
            take(8)
        count = integer()
        if count > len(data) - pos:
            raise ValueError("tabela de upvalues Lua inválida")
        for _ in range(count):
            string()

    try:
        function(0, "0")
        return matches if pos == len(data) else []
    except (ValueError, struct.error):
        return []
