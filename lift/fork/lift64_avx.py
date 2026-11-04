"""lift64_avx.py — SuperBonn fork do lifter 64-bit (pcrecomp).

Cobre o que o censo mediu nos binarios do Megabonk (docs/RECON-DAY1.md §4):

  1. VEX 128-bit escalar/packed (addss..vfmadd*, vmovsd, vcomisd, ...) e
     packed-inteiro comun (vpand/vpxor/vpaddq/vpsubd, cvtdq2pd, ...)
  2. AVX-256 (ymm) decomposto em duas metades de 128 bits — o runtime guarda
     ymm[16] ao lado de xmm[16] (patch em runtime/recomp64_cpu/cpu64.h)
  3. SSE2 packed-double (sqrtpd/addpd/andpd/...), hoje TODO
  4. `int 0x29` (check CFG do VS2022), `xgetbv`, `sfence` — no-ops seguros
     no host

VEX e 3-operando: `vop dst, src1, src2` => dst = op(src1, src2).
Regra de bits altos (VEX.128 escalar): DEST[127:elemento] vem de SRC1.

Regra de ouro deste fork: TODO desvio devolve None (nunca levanta errado) —
a nao ser que a semantica seja trivialmente decomponivel. Qualquer forma que
nao reconhecemos devolve None e o lifter original emite o RECOMP_TODO com
nome e endereco, como antes.

Uso: importado pelo patch em lift64_cpu.py (scripts/lift/apply-fork.py).
"""

from capstone.x86 import X86_OP_IMM, X86_OP_REG

# (operador C) para aritmetica escalar/packed VEX; ss -> float, sd -> double.
VEX_SCALAR_ARITH = {}
VEX_PACKED_ARITH = {}
for _op in ('add', 'sub', 'mul', 'div', 'min', 'max'):
    VEX_SCALAR_ARITH['v' + _op + 'ss'] = (_op, 'ss')
    VEX_SCALAR_ARITH['v' + _op + 'sd'] = (_op, 'sd')
    VEX_PACKED_ARITH['v' + _op + 'ps'] = (_op, 'ps')
    VEX_PACKED_ARITH['v' + _op + 'pd'] = (_op, 'pd')

VEX_BITWISE = {}
for _op in ('xor', 'and', 'or', 'andn'):
    VEX_BITWISE['v' + _op + 'ps'] = (_op, 'ps')
    VEX_BITWISE['v' + _op + 'pd'] = (_op, 'pd')

VEX_PACKED_INT = {          # (lanes, campo, operador)
    'vpxor': ('u64', 2, '^'), 'vpand': ('u64', 2, '&'), 'vpor': ('u64', 2, '|'),
    'vpaddd': ('u32', 4, '+'), 'vpsubd': ('u32', 4, '-'),
    'vpaddq': ('u64', 2, '+'), 'vpsubq': ('u64', 2, '-'),
}

MOV128 = ('movaps', 'movups', 'movapd', 'movupd', 'movdqa', 'movdqu')

# FMA: ordem dos fatores nas TRES formas. Resultado sempre no dest.
FMA_ORDERS = {
    '132': ('d', 's2', 's1'),   # dst = dst * s2  + s1
    '213': ('s1', 's2', 'd'),   # dst = s1  * s2  + dst
    '231': ('s1', 'd', 's2'),   # dst = s1  * dst + s2
}


def _is_ymm(L, op):
    return op.type == X86_OP_REG and L.rname(op.reg).startswith('ymm')


def _yi(L, op):
    name = L.rname(op.reg) if op.type == X86_OP_REG else '<mem>'
    if not name.startswith('ymm') or not name[3:].isdigit():
        raise NotImplementedError('not a ymm operand: %s' % name)
    return int(name[3:])


def _lane(L, insn, op, wide):
    """Expressao C do lane escalar de um operando (reg ou memoria)."""
    return L._sd(insn, op) if wide else L._ss(insn, op)


def _fma_expr(wide, sub, neg, a, b, c):
    f = 'fma' if wide else 'fmaf'
    if sub:                       # ±(a*b) ∓ c
        return '%s(%s, %s, %s%s)' % (f, a, ('-' if neg else '') + b,
                                     '-' if not neg else '', c)
    return '%s(%s, %s, %s%s)' % (f, a, b, ('-' if neg else '') + '', c) \
        if False else '%s(%s, %s, %s)' % (f, a, b, ('-' + c) if neg else c)


def _scalar_vex(L, insn, m, ops, ea):
    """VEX 128-bit: escalar aritmetico, sqrt, moves, compares, FMA, packed."""
    vbase = m[1:]

    if len(ops) == 3 and m in VEX_SCALAR_ARITH:
        op, suf = VEX_SCALAR_ARITH[m]
        wide = suf == 'sd'
        d, s1, s2 = ops
        if not L._is_xmm(d):
            return None
        n = L._xi(d)
        a, b = _lane(L, insn, s1, wide), _lane(L, insn, s2, wide)
        if op == 'min':
            expr = 'sse_min%s(%s, %s)' % ('d' if wide else 's', a, b)
        elif op == 'max':
            expr = 'sse_max%s(%s, %s)' % ('d' if wide else 's', a, b)
        else:
            expr = '(%s %s %s)' % (a, op, b)
        out = ['c->xmm[%d].%s[0] = %s;' % (n, 'f64' if wide else 'f32', expr)]
        if L._is_xmm(s1):
            out.append('c->xmm[%d].u64[1] = c->xmm[%d].u64[1];' % (n, L._xi(s1)))
        return out

    if len(ops) == 3 and vbase in ('sqrtss', 'sqrtsd'):
        wide = vbase.endswith('sd')
        d, s1, s2 = ops
        if not L._is_xmm(d):
            return None
        n = L._xi(d)
        out = ['c->xmm[%d].%s[0] = sqrt%s(%s);'
               % (n, 'f64' if wide else 'f32', 'd' if wide else 's',
                  _lane(L, insn, s2, wide))]
        if L._is_xmm(s1):
            out.append('c->xmm[%d].u64[1] = c->xmm[%d].u64[1];' % (n, L._xi(s1)))
        return out

    if len(ops) == 3 and m in VEX_PACKED_ARITH:
        op, suf = VEX_PACKED_ARITH[m]
        wide = suf == 'pd'
        d, s1, s2 = ops
        if not L._is_xmm(d):
            return None
        n = L._xi(d)
        fld, nlanes = ('f64', 2) if wide else ('f32', 4)
        ext = 'd' if wide else 's'
        out = ['{ XMM _v = %s, _w = %s;' % (L._xm(insn, s1), L._xm(insn, s2))]
        for i in range(nlanes):
            if op == 'min':
                e = 'sse_min%s(_v.%s[%d], _w.%s[%d])' % (ext, fld, i, fld, i)
            elif op == 'max':
                e = 'sse_max%s(_v.%s[%d], _w.%s[%d])' % (ext, fld, i, fld, i)
            else:
                e = '(_v.%s[%d] %s _w.%s[%d])' % (fld, i, op, fld, i)
            out.append(' c->xmm[%d].%s[%d] = %s;' % (n, fld, i, e))
        out.append('}')
        return out

    if len(ops) == 3 and m in VEX_BITWISE:
        op, suf = VEX_BITWISE[m]
        wide = suf == 'pd'
        d, s1, s2 = ops
        if not L._is_xmm(d):
            return None
        n = L._xi(d)
        fld, nlanes = ('u64', 2) if wide else ('u32', 4)
        out = ['{ XMM _v = %s, _w = %s;' % (L._xm(insn, s1), L._xm(insn, s2))]
        for i in range(nlanes):
            if op == 'andn':
                e = '((~_v.%s[%d]) & _w.%s[%d])' % (fld, i, fld, i)
            else:
                e = '(_v.%s[%d] %s _w.%s[%d])' % (fld, i, op, fld, i)
            out.append(' c->xmm[%d].%s[%d] = %s;' % (n, fld, i, e))
        out.append('}')
        return out

    if len(ops) == 3 and m in VEX_PACKED_INT:
        fld, nlanes, op = VEX_PACKED_INT[m]
        d, s1, s2 = ops
        if not L._is_xmm(d):
            return None
        n = L._xi(d)
        out = ['{ XMM _v = %s, _w = %s;' % (L._xm(insn, s1), L._xm(insn, s2))]
        for i in range(nlanes):
            out.append(' c->xmm[%d].%s[%d] = (_v.%s[%d] %s _w.%s[%d]);'
                       % (n, fld, i, fld, i, op, fld, i))
        out.append('}')
        return out

    # moves 128-bit (VEX): dst = src2 quando 3-op, senão src1
    if vbase in MOV128:
        d = ops[0]
        s = ops[2] if len(ops) >= 3 else ops[1]
        if L._is_xmm(d):
            return ['c->xmm[%d] = %s;' % (L._xi(d), L._xm(insn, s))]
        return ['wrxm(%s, c->xmm[%d]);' % (L.addr_expr(insn, d), L._xi(s))]

    # moves escalares vmovss/vmovsd
    if vbase in ('movss', 'movsd'):
        wide = vbase.endswith('sd')
        d, s1 = ops[0], ops[1]
        s2 = ops[2] if len(ops) >= 3 else None
        fld = 'f64' if wide else 'f32'
        if L._is_xmm(d) and s2 is not None and L._is_xmm(s2):
            n = L._xi(d)
            return ['c->xmm[%d].%s[0] = c->xmm[%d].%s[0]; '
                    'c->xmm[%d].u64[1] = c->xmm[%d].u64[1];'
                    % (n, fld, L._xi(s2), fld, n, L._xi(s1))]
        if L._is_xmm(d):
            n = L._xi(d)
            return ['sse_load_s%s(&c->xmm[%d], %s);'
                    % ('d' if wide else 's', n, _lane(L, insn, s1, wide))]
        st = 'wrsd' if wide else 'wrss'
        return ['%s(%s, %s);' % (st, L.addr_expr(insn, d),
                                 _lane(L, insn, s1, wide))]

    # movd/movq VEX
    if vbase == 'movd':
        d, s1 = ops[0], ops[1]
        if L._is_xmm(d):
            n = L._xi(d)
            return ['c->xmm[%d].u64[0] = (uint32_t)(%s); c->xmm[%d].u64[1] = 0;'
                    % (n, L.src(insn, s1), n)]
        return [L.dst_write(insn, d, 'c->xmm[%d].u32[0]' % L._xi(s1))]
    if vbase == 'movq':
        d, s1 = ops[0], ops[1]
        if L._is_xmm(d):
            n = L._xi(d)
            return ['c->xmm[%d].u64[0] = %s; c->xmm[%d].u64[1] = 0;'
                    % (n, L.src(insn, s1), n)]
        return [L.dst_write(insn, d, 'c->xmm[%d].u64[0]' % L._xi(s1))]

    # compares que setam flags
    if len(ops) == 2 and vbase in ('comiss', 'ucomiss', 'comisd', 'ucomisd'):
        d, s = ops
        rd_ = L._sd if vbase.endswith('sd') else L._ss
        return ['sse_compare(c, %s, %s);' % (rd_(insn, d), rd_(insn, s))]

    # conversoes
    if len(ops) == 3 and vbase in ('cvtdq2pd', 'cvtdq2ps', 'cvtpd2ps',
                                   'cvtps2pd', 'cvtpd2dq', 'cvtps2dq'):
        d, s1, s2 = ops
        if not L._is_xmm(d):
            return None
        n = L._xi(d)
        if vbase == 'cvtdq2pd':
            src = L._xi(s2) if L._is_xmm(s2) else None
            if src is None:
                return None
            return ['c->xmm[%d].f64[0] = (double)(int32_t)c->xmm[%d].i32[0]; '
                    'c->xmm[%d].f64[1] = (double)(int32_t)c->xmm[%d].i32[1];'
                    % (n, src, n, src)]
        if vbase == 'cvtdq2ps':
            src = L._xi(s2) if L._is_xmm(s2) else None
            if src is None:
                return None
            return ['c->xmm[%d].f32[0] = (float)(int32_t)c->xmm[%d].i32[0]; '
                    'c->xmm[%d].f32[1] = (float)(int32_t)c->xmm[%d].i32[1]; '
                    'c->xmm[%d].f32[2] = (float)(int32_t)c->xmm[%d].i32[2]; '
                    'c->xmm[%d].f32[3] = (float)(int32_t)c->xmm[%d].i32[3];'
                    % (n, src, n, src, n, src, n, src)]
        return None

    # FMA (vfmadd132sd / vfnmadd231ss / vfmsub231pd / ...)
    if m.startswith(('vfmadd', 'vfnmadd', 'vfmsub', 'vfnmsub')):
        if len(ops) < 3:
            return None
        neg = m.startswith('vfnmadd') or m.startswith('vfnmsub')
        sub = m.startswith('vfmsub') or m.startswith('vfnmsub')
        order = m[-5:-2]          # '132' | '213' | '231'
        wide = m.endswith('sd')
        if order not in FMA_ORDERS:
            return None
        d, s1, s2 = ops[0], ops[1], ops[2]
        if not L._is_xmm(d):
            return None
        n = L._xi(d)
        cop = {'d': 'c->xmm[%d].f64[0]' % n,
               's1': _lane(L, insn, s1, True),
               's2': _lane(L, insn, s2, True)}
        a_key, b_key, c_key = FMA_ORDERS[order]
        expr = _fma_expr(wide, sub, neg, cop[a_key], cop[b_key], cop[c_key])
        out = ['c->xmm[%d].%s[0] = %s;' % (n, 'f64' if wide else 'f32', expr)]
        if L._is_xmm(s1):
            out.append('c->xmm[%d].u64[1] = c->xmm[%d].u64[1];' % (n, L._xi(s1)))
        return out

    # packed shift inteiro VEX
    if vbase in ('psrlq', 'psllq', 'psrld', 'pslld'):
        d, s1 = ops[0], ops[1]
        s2 = ops[2] if len(ops) >= 3 else None
        cnt = (s2 if s2 is not None else s1).imm & 0xFF
        if not L._is_xmm(d):
            return None
        n = L._xi(d)
        width = {'psrlq': 64, 'psllq': 64, 'psrld': 32, 'pslld': 32}[vbase]
        fld = 'u64' if vbase.endswith('q') else 'u32'
        lanes = 2 if fld == 'u64' else 4
        if cnt >= width:
            return ['c->xmm[%d] = (XMM){{0}};' % n]
        opc = '>>' if vbase.startswith('psrl') else '<<'
        return ['c->xmm[%d].%s[%d] %s= UINT64_C(%d); c->xmm[%d].%s[%d] %s= UINT64_C(%d);'
                % (n, fld, 0, opc, cnt, n, fld, 1, opc, cnt)] if lanes == 2 else \
               ['c->xmm[%d].%s[%d] %s= UINT64_C(%d); c->xmm[%d].%s[%d] %s= UINT64_C(%d);'
                'c->xmm[%d].%s[%d] %s= UINT64_C(%d); c->xmm[%d].%s[%d] %s= UINT64_C(%d);'
                % (n, fld, 0, opc, cnt, n, fld, 1, opc, cnt,
                   n, fld, 2, opc, cnt, n, fld, 3, opc, cnt)]

    # movntdq (store nao-temporal): entrega a semantica de store normal
    if vbase in ('movntdq', 'movntps', 'movntpd'):
        n = _yi(L, ops[0]) if _is_ymm(L, ops[0]) else L._xi(ops[0])
        addr = L.addr_expr(insn, ops[0])
        if _is_ymm(L, ops[0]):
            return ['{ uint64_t _a = %s; wrxm(_a, c->xmm[%d]); '
                    'wrxm(_a + 16, c->ymm[%d]); }' % (addr, n, n)]
        return ['wrxm(%s, c->xmm[%d]);' % (addr, n)]

    # conversoes escalares VEX: dst = cvt(src2), bits altos de src1
    if len(ops) == 3 and vbase in ('cvtsd2ss', 'cvtss2sd'):
        d, s1, s2 = ops
        if not L._is_xmm(d):
            return None
        n = L._xi(d)
        if vbase == 'cvtsd2ss':
            return ['c->xmm[%d].f32[0] = (float)%s; '
                    'c->xmm[%d].u32[1] = c->xmm[%d].u32[1]; '
                    'c->xmm[%d].u64[1] = c->xmm[%d].u64[1];'
                    % (n, L._sd(insn, s2), n, L._xi(s1) if L._is_xmm(s1) else n, n,
                       L._xi(s1) if L._is_xmm(s1) else n)]
        return ['c->xmm[%d].f64[0] = (double)%s; '
                'c->xmm[%d].u64[1] = c->xmm[%d].u64[1];'
                % (n, L._ss(insn, s2), n, L._xi(s1) if L._is_xmm(s1) else n)]

    # movmsk: extrai os bits de sinal para um GPR
    if vbase in ('movmskpd', 'movmskps'):
        d, s = ops[0], ops[1]
        n = L._xi(s)
        if vbase == 'movmskpd':
            expr = '((c->xmm[%d].u64[0] >> 63) | ((c->xmm[%d].u64[1] >> 63) << 1))' % (n, n)
        else:
            expr = ('((c->xmm[%d].u32[0] >> 31) | ((c->xmm[%d].u32[1] & 0x80000000u) >> 30) | '
                    '((c->xmm[%d].u32[2] & 0x80000000u) >> 29) | '
                    '((c->xmm[%d].u32[3] & 0x80000000u) >> 28))') % (n, n, n, n)
        return [L.dst_write(insn, d, expr)]

    if vbase == 'pmovmskb' and len(ops) == 2:
        d, s = ops
        n = L._xi(s)
        expr = ('((c->xmm[%d].u32[0] >> 7) & 1) | (((c->xmm[%d].u32[0] >> 15) & 1) << 1) | '
                '(((c->xmm[%d].u32[0] >> 23) & 1) << 2) | (((c->xmm[%d].u32[0] >> 31) & 1) << 3) | '
                '((c->xmm[%d].u32[1] >> 7) & 1) << 4 | (((c->xmm[%d].u32[1] >> 15) & 1) << 5) | '
                '(((c->xmm[%d].u32[1] >> 23) & 1) << 6) | (((c->xmm[%d].u32[1] >> 31) & 1) << 7)') \
               % (n, n, n, n, n, n, n, n, n)
        return [L.dst_write(insn, d, expr)]

    return None


def _scalar_ymm(L, insn, m, ops, ea, vbase):
    """AVX-256: ymmN = xmmN (metade baixa) + c->ymm[N] (metade alta)."""
    if vbase in MOV128:
        d, s = ops[0], ops[1]
        if len(ops) >= 3:
            s = ops[2]
        if _is_ymm(L, d) and _is_ymm(L, s):
            return ['c->xmm[%d] = c->xmm[%d]; c->ymm[%d] = c->ymm[%d];'
                    % (_yi(L, d), _yi(L, s), _yi(L, d), _yi(L, s))]
        if _is_ymm(L, d):
            n = _yi(L, d)
            return ['{ uint64_t _a = %s; c->xmm[%d] = rdxm(_a); '
                    'c->ymm[%d] = rdxm(_a + 16); }'
                    % (L.addr_expr(insn, s), n, n)]
        if _is_ymm(L, s):
            n = _yi(L, s)
            return ['{ uint64_t _a = %s; wrxm(_a, c->xmm[%d]); '
                    'wrxm(_a + 16, c->ymm[%d]); }'
                    % (L.addr_expr(insn, d), n, n)]
        return None

    if vbase == 'insertf128' and len(ops) >= 3:
        d, s1, s2 = ops[0], ops[1], ops[2]
        imm = ops[3].imm if len(ops) > 3 else 0
        if not (_is_ymm(L, d) and _is_ymm(L, s1) and L._is_xmm(s2)):
            return None
        n, n1 = _yi(L, d), _yi(L, s1)
        if imm == 1:
            return ['c->ymm[%d] = c->xmm[%d];' % (n, L._xi(s2))]
        return ['c->xmm[%d] = c->xmm[%d]; c->ymm[%d] = c->ymm[%d];'
                % (n, L._xi(s2), n, n1)]

    if vbase == 'extractf128' and len(ops) >= 2:
        d, s = ops[0], ops[1]
        imm = ops[2].imm if len(ops) > 2 else 0
        if _is_ymm(L, d) or not _is_ymm(L, s):
            return None
        n = L._xi(d)
        return ['c->xmm[%d] = c->ymm[%d];' % (n, _yi(L, s))] if imm == 1 \
            else ['c->xmm[%d] = c->xmm[%d];' % (n, _yi(L, s))]

    if vbase == 'movntdq':
        d, s = ops[0], ops[1]
        if _is_ymm(L, s):                       # store nao-temporal
            n = _yi(L, s)
            return ['{ uint64_t _a = %s; wrxm(_a, c->xmm[%d]); '
                    'wrxm(_a + 16, c->ymm[%d]); }'
                    % (L.addr_expr(insn, d), n, n)]
        if _is_ymm(L, d):                       # load nao-temporal
            n = _yi(L, d)
            return ['{ uint64_t _a = %s; c->xmm[%d] = rdxm(_a); '
                    'c->ymm[%d] = rdxm(_a + 16); }'
                    % (L.addr_expr(insn, s), n, n)]
        return None

    # aritmetica/bitwise packed sobre ymm: duas metades
    if m in VEX_PACKED_ARITH and len(ops) == 3:
        op, suf = VEX_PACKED_ARITH[m]
        d, s1, s2 = ops
        if not (_is_ymm(L, d) and _is_ymm(L, s1) and _is_ymm(L, s2)):
            return None
        nd, n1, n2 = _yi(L, d), _yi(L, s1), _yi(L, s2)
        wide = suf == 'pd'
        fld, nlanes = ('f64', 2) if wide else ('f32', 4)
        ext = 'd' if wide else 's'
        out = []
        for arr in ('c->xmm[%d]', 'c->ymm[%d]'):
            for i in range(nlanes):
                if op == 'min':
                    e = 'sse_min%s(%s.%s[%d], %s.%s[%d])' % (ext, arr % n1, fld, i, arr % n2, fld, i)
                elif op == 'max':
                    e = 'sse_max%s(%s.%s[%d], %s.%s[%d])' % (ext, arr % n1, fld, i, arr % n2, fld, i)
                else:
                    e = '(%s.%s[%d] %s %s.%s[%d])' % (arr % n1, fld, i, op, arr % n2, fld, i)
                out.append('%s.%s[%d] = %s;' % (arr % nd, fld, i, e))
        return out

    if m in VEX_BITWISE and len(ops) == 3:
        op, suf = VEX_BITWISE[m]
        d, s1, s2 = ops
        if not (_is_ymm(L, d) and _is_ymm(L, s1) and _is_ymm(L, s2)):
            return None
        nd, n1, n2 = _yi(L, d), _yi(L, s1), _yi(L, s2)
        fld, nlanes = ('u64', 2) if suf == 'pd' else ('u32', 4)
        out = []
        for arr in ('c->xmm[%d]', 'c->ymm[%d]'):
            for i in range(nlanes):
                if op == 'andn':
                    e = '((~%s.%s[%d]) & %s.%s[%d])' % (arr % n1, fld, i, arr % n2, fld, i)
                else:
                    e = '(%s.%s[%d] %s %s.%s[%d])' % (arr % n1, fld, i, op, arr % n2, fld, i)
                out.append('%s.%s[%d] = %s;' % (arr % nd, fld, i, e))
        return out

    if m in VEX_PACKED_INT and len(ops) == 3:
        fld, nlanes, op = VEX_PACKED_INT[m]
        d, s1, s2 = ops
        if not (_is_ymm(L, d) and _is_ymm(L, s1) and _is_ymm(L, s2)):
            return None
        nd, n1, n2 = _yi(L, d), _yi(L, s1), _yi(L, s2)
        out = []
        for arr in ('c->xmm[%d]', 'c->ymm[%d]'):
            for i in range(nlanes):
                out.append('%s.%s[%d] = (%s.%s[%d] %s %s.%s[%d]);'
                           % (arr % nd, fld, i, arr % n1, fld, i, op, arr % n2, fld, i))
        return out

    return None


def _sse_packed_double(L, insn, m, ops, ea):
    """SSE2 packed-double nao-VEX (sqrtpd/addpd/...): hoje TODO no lifter."""
    if len(ops) < 2:
        return None
    d, s = ops[0], ops[1]
    if not L._is_xmm(d):
        return None
    n = L._xi(d)
    if m == 'sqrtpd':
        return ['{ XMM _s = %s; c->xmm[%d].f64[0] = sqrt(_s.f64[0]); '
                'c->xmm[%d].f64[1] = sqrt(_s.f64[1]); }' % (L._xm(insn, s), n, n)]
    if m in ('addpd', 'subpd', 'mulpd', 'divpd', 'maxpd', 'minpd'):
        op = m[:-2]
        fn = {'maxpd': 'sse_maxd', 'minpd': 'sse_mind'}.get(m)
        out = ['{ XMM _w = %s;' % L._xm(insn, s)]
        for i in range(2):
            if fn:
                e = '%s(c->xmm[%d].f64[%d], _w.f64[%d])' % (fn, n, i, i)
            else:
                e = '(c->xmm[%d].f64[%d] %s _w.f64[%d])' % (n, i, op, i)
            out.append(' c->xmm[%d].f64[%d] = %s;' % (n, i, e))
        out.append('}')
        return out
    if m in ('andpd', 'andnpd', 'orpd', 'xorpd'):
        op = m[:-2]
        out = ['{ XMM _w = %s;' % L._xm(insn, s)]
        for i in range(2):
            if op == 'andn':
                e = '((~c->xmm[%d].u64[%d]) & _w.u64[%d])' % (n, i, i)
            else:
                e = '(c->xmm[%d].u64[%d] %s _w.u64[%d])' % (n, i, op, i)
            out.append(' c->xmm[%d].u64[%d] = %s;' % (n, i, e))
        out.append('}')
        return out
    return None


def _sad(L, insn, m, ops, ea, vbase):
    """SAD: psadbw/mpsadbw — SSE (nao-VEX) e VEX. Caminho compartilhado."""
    if vbase == 'psadbw' and len(ops) == 2:
        d, s = ops
        if not L._is_xmm(d):
            return None
        n = L._xi(d)
        return ['{ const uint8_t* _pa = (const uint8_t*)&c->xmm[%d]; '
                'XMM _w = %s; const uint8_t* _pb = (const uint8_t*)&_w; '
                'unsigned _s0 = 0, _s1 = 0; '
                'for (int _i = 0; _i < 8; ++_i) { '
                '  int _d0 = _pa[_i] - _pb[_i]; _s0 += (_d0 < 0 ? -_d0 : _d0); '
                '  int _d1 = _pa[8 + _i] - _pb[8 + _i]; _s1 += (_d1 < 0 ? -_d1 : _d1); } '
                'c->xmm[%d].u64[0] = _s0; c->xmm[%d].u64[1] = _s1; }'
                % (n, L._xm(insn, s), n, n)]

    if vbase == 'mpsadbw' and len(ops) == 3:
        d, s1, s2 = ops
        if not L._is_xmm(d) or s2.type != X86_OP_IMM:
            return None
        imm = s2.imm
        if (imm >> 3) & 1:
            return None                     # seletor cruzado: nao modelado
        off = imm & 3
        n = L._xi(d)
        ha1 = 8 if (imm >> 2) & 1 else 0
        ha2 = 0 if (imm >> 2) & 1 else 8
        return ['{ const uint8_t* _pa = (const uint8_t*)&c->xmm[%d]; '
                'XMM _w = %s; const uint8_t* _pb = (const uint8_t*)&_w; '
                'unsigned _s0 = 0, _s1 = 0; '
                'for (int _i = 0; _i < 8; ++_i) { '
                '  int _a0 = _pa[%d + _i] - _pb[%d + _i]; _s0 += (_a0 < 0 ? -_a0 : _a0); '
                '  int _a1 = _pa[%d + _i] - _pb[%d + _i]; _s1 += (_a1 < 0 ? -_a1 : _a1); } '
                'c->xmm[%d].u64[0] = _s0; c->xmm[%d].u64[1] = _s1; }'
                % (n, L._xm(insn, s1), ha1 + off, ha1 + off,
                   ha2 + off, ha2 + off, n, n)]
    return None


def lift_avx(L, insn, m, ops, ea):
    # ---- 1. no-ops seguros no host ----------------------------------------
    if m == 'int' and len(ops) == 1 and ops[0].type == X86_OP_IMM \
            and ops[0].imm == 0x29:
        return ['/* CFE check int 0x29: no-op no host */']

    if m == 'vzeroupper':
        return ['memset(c->ymm, 0, sizeof(c->ymm)); /* vzeroupper */']

    if m == 'xgetbv':
        return ['c->rax = UINT64_C(7); c->rdx = 0; /* xgetbv: XCR0 AVX+SSE */']

    if m == 'sfence' or m == 'lfence' or m == 'mfence':
        return ['/* %s: barreira; o compilador do host ja ordena */' % m]

    if m == 'bswap' and len(ops) == 1:
        w = '64' if ops[0].size == 8 else '32'
        return [L.dst_write(insn, ops[0], '__builtin_bswap%s(%s)'
                            % (w, L.src(insn, ops[0])))]

    # ---- 2. SSE2 packed-double nao-VEX ------------------------------------
    if not m.startswith('v'):
        return _sse_packed_double(L, insn, m, ops, ea)

    # ---- 3. ymm (AVX-256): duas metades de 128 ----------------------------
    if any(_is_ymm(L, o) for o in ops):
        out = _scalar_ymm(L, insn, m, ops, ea, m[1:])
        if out is not None:
            return out
        return None

    # ---- 3. SAD (comum a SSE e VEX) ----------------------------------------
    vbase = m[1:] if m.startswith('v') else m
    sad = _sad(L, insn, m, ops, ea, vbase)
    if sad is not None:
        return sad

    # ---- 4. SSE2 packed-double nao-VEX ------------------------------------
    if not m.startswith('v'):
        return _sse_packed_double(L, insn, m, ops, ea)

    # ---- 5. VEX 128-bit ----------------------------------------------------
    return _scalar_vex(L, insn, m, ops, ea)
