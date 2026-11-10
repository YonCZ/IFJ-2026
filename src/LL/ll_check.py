#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ll_check.py - mechanická LL verifikace gramatiky IFJ26 (anglická notace, v7)
+ trasování kritických tokenových streamů (případy A–G, koncová čárka, kolize id…).

Pseudoterminály Expr / ExprNoId / ExprFromId modelují rozhraní s výrazovou
(precedenční) analýzou: stavový automat operand⇄operátor s hloubkou závorek.
LPAREN po operandu výraz ukončuje - volání ve výrazu v základu neexistuje.
RPAREN! = uzavírací závorka se nastaveným EOL-flagem (varianta b).

"""

# ---------------------------- gramatika ----------------------------
# (neterminál, pravá strana) - prázdná tuple = ε
P = [
    ("Program",       ("Prolog", "FuncDefs", "EOF")),
    ("Prolog",        ("IMPORT", "ID", "DOT", "MUL", "EOL")),
    ("FuncDefs",      ("EOL", "FuncDefs")),
    ("FuncDefs",      ("FuncDef", "FuncDefs")),
    ("FuncDefs",      ()),
    ("FuncDef",       ("DEF", "ID", "Params", "RetType", "ASSIGN", "Body")),
    ("FuncDef",       ("AT", "ID", "OptEol", "DEF", "ID", "Params", "RetType", "ASSIGN", "Body")),
    # v7: anotace @main může být na samostatném řádku (§2 PDF) - OptEol = EOL | ε
    ("OptEol",        ("EOL",)),
    ("OptEol",        ()),
    ("Params",        ("LPAREN", "ParamList", "RPAREN")),
    ("ParamList",     ("Param", "ParamRest")),
    ("ParamList",     ()),
    ("ParamRest",     ("COMMA", "AfterCommaParam")),
    ("ParamRest",     ()),
    ("AfterCommaParam", ("Param", "ParamRest")),
    ("AfterCommaParam", ()),
    ("Param",         ("ID", "COLON", "Type")),
    ("RetType",       ("COLON", "Type")),
    ("RetType",       ()),
    ("Type",          ("INT_TOKEN",)),
    ("Type",          ("DOUBLE_TOKEN",)),
    ("Type",          ("STRING_TOKEN",)),
    ("Type",          ("UNIT_TOKEN",)),
    ("Body",          ("Rhs", "EOL")),
    ("Body",          ("EOL", "INDENT", "Block", "DEDENT")),
    ("Block",         ("BlockItem", "StmtRest")),
    ("BlockItem",     ("SimpleStmt", "EndSimple")),
    ("BlockItem",     ("IfStmt",)),
    ("BlockItem",     ("WhileStmt",)),
    ("StmtRest",      ("EOL", "StmtRest")),
    ("StmtRest",      ("BlockItem", "StmtRest")),
    ("StmtRest",      ()),
    ("EndSimple",     ("EOL",)),
    ("EndSimple",     ("SEMICOLON",)),
    ("SimpleStmt",    ("ValStmt",)),
    ("SimpleStmt",    ("VarStmt",)),
    ("SimpleStmt",    ("ReturnStmt",)),
    ("SimpleStmt",    ("ID", "StmtIdTail")),
    ("SimpleStmt",    ("ExprNoId",)),
    ("ValStmt",       ("VAL", "ID", "ValVarTail")),
    ("VarStmt",       ("VAR", "ID", "ValVarTail")),
    # v4: Newlines smazán - typ a = na jednom logickém řádku (§1.2, grammar.md §4.1)
    ("ValVarTail",    ("COLON", "Type", "ASSIGN", "Rhs")),
    ("ValVarTail",    ("ASSIGN", "Rhs")),
    ("IfStmt",        ("IF", "Expr", "THEN", "EOL", "INDENT", "Block", "DEDENT",
                       "ELSE", "EOL", "INDENT", "Block", "DEDENT")),
    ("WhileStmt",     ("WHILE", "Expr", "DO", "EOL", "INDENT", "Block", "DEDENT")),
    # v5: return umí i volání (strings.scala) - Rhs místo Expr; ε = holé return (otázka 1)
    ("ReturnStmt",    ("RETURN", "RhsOpt")),
    ("RhsOpt",        ("Rhs",)),
    ("RhsOpt",        ()),
    ("Rhs",           ("ID", "IdTail")),
    ("Rhs",           ("ExprNoId",)),
    ("IdTail",        ("LPAREN", "ArgList", "RPAREN")),
    ("IdTail",        ("ExprFromId",)),
    ("StmtIdTail",    ("ASSIGN", "Rhs")),
    ("StmtIdTail",    ("IdTail",)),
    ("ArgList",       ("Term", "ArgRest")),
    ("ArgList",       ()),
    ("ArgRest",       ("COMMA", "AfterCommaArg")),
    ("ArgRest",       ()),
    ("AfterCommaArg", ("Term", "ArgRest")),
    ("AfterCommaArg", ()),
    ("Term",          ("ID",)),
    ("Term",          ("INT_NUM",)),
    ("Term",          ("DOUBLE_NUM",)),
    ("Term",          ("STRING_LITERAL",)),
    ("Term",          ("LPAREN", "RPAREN")),    # v7: () = literál Unit (§1.1 PDF)
]

NT = {a for a, _ in P}

E0 = {"ID", "INT_NUM", "DOUBLE_NUM", "STRING_LITERAL", "LPAREN", "PLUS", "MINUS"}
# interface pseudoterminály - mají FIRST, ale nemají vlastní pravidla
PSEUDO = {
    "Expr":        set(E0),                                   # výraz od aktuálního tokenu
    "ExprNoId":    set(E0) - {"ID"},                          # totéž, volající zaručuje začátek != ID
    "ExprFromId":  {"PLUS", "MINUS", "MUL", "DIV", "LT", "LE", "GT", "GE",
                    "EQ", "NEQ", "EOL", "SEMICOLON"},  # od uloženého ID; FOLLOW = {EOL,SEMI}
                                                      # (THEN/DO vypuštěny - mrtvé buňky, v4)
}
TERMS = {s for _, r in P for s in r if s not in NT}
for k in PSEUDO:
    assert k in TERMS, f"pseudoterminal {k} se nevyskytuje v pravidlech"

# ------------------- nullable / FIRST / FOLLOW -------------------
nullable = set()
chg = True
while chg:
    chg = False
    for a, r in P:
        if a in nullable:
            continue
        if len(r) == 0 or all(s in nullable for s in r):
            nullable.add(a)
            chg = True

first = {a: set() for a in NT}

def seq_first(rhs):
    """FIRST sekvence symbolů → (množina, je_cela_nullable)"""
    out = set()
    for s in rhs:
        if s in PSEUDO:
            return out | PSEUDO[s], False
        if s in TERMS:
            return out | {s}, False
        out |= first[s] - {"EPS"}
        if "EPS" not in first[s]:
            return out, False
    return out, True

chg = True
while chg:
    chg = False
    for a, r in P:
        f, n = seq_first(r)
        if n:
            f = f | {"EPS"}
        if not f <= first[a]:
            first[a] |= f
            chg = True

follow = {a: set() for a in NT}
chg = True
while chg:
    chg = False
    for a, r in P:
        for i, s in enumerate(r):
            if s not in NT:
                continue
            f, n = seq_first(r[i + 1:])
            if not f <= follow[s]:
                follow[s] |= f
                chg = True
            if n and not follow[a] <= follow[s]:
                follow[s] |= follow[a]
                chg = True

# ---------------------------- LL tabulka ----------------------------
table = {}
collisions = []
for a, r in P:
    sel, n = seq_first(r)
    sel = sel - {"EPS"}
    if n:
        sel = sel | follow[a]
    for t in sel:
        if (a, t) in table and table[(a, t)] != r:
            collisions.append((a, t, table[(a, t)], r))
        table[(a, t)] = r

# ---------------------------- parser ----------------------------
OPS = {"PLUS", "MINUS", "MUL", "DIV", "LT", "LE", "GT", "GE", "EQ", "NEQ"}
RELOPS = {"LT", "LE", "GT", "GE", "EQ", "NEQ"}
OPERANDS = {"ID", "INT_NUM", "DOUBLE_NUM", "STRING_LITERAL"}

def greedy(toks, i, kind):
    """Aproximace výrazové analýzy: konzumuje výraz, na stopping tokenu se
    zastaví BEZ konzumace. kind=ExprFromId → začíná ve stavu 'operátor'
    (ID už přečteno volajícím). Vrací (index, platnost) - platnost = False
    znamená syntakticky neúplný/neplatný výraz."""
    depth = 0
    inside = [0]      # počet tokenů uvnitř úrovně závorek (detekce (1 +))
    rels = [0]        # relační operátory na úrovni (řetězení zakázáno, §3)
    expecting = "operand" if kind != "ExprFromId" else "operator"
    while i < len(toks):
        t = toks[i][0]
        if expecting == "operand":
            if t in OPERANDS:
                i += 1
                inside[-1] += 1
                expecting = "operator"
            elif t in ("PLUS", "MINUS"):          # unární +/-
                i += 1
                inside[-1] += 1
            elif t == "LPAREN":
                i += 1
                depth += 1
                inside.append(0)
                rels.append(0)
            elif t == "RPAREN" and depth > 0:      # () jako Unit literál
                if inside[-1] != 0:                # závorka s nedokončeným obsahem
                    return i, False
                i += 1
                depth -= 1
                inside.pop()
                rels.pop()
                expecting = "operator"
            else:
                break
        else:                                       # čekáme operátor nebo konec
            if t in OPS:
                i += 1
                inside[-1] += 1
                if t in RELOPS:
                    rels[-1] += 1
                expecting = "operand"
            elif t == "RPAREN" and depth > 0:
                if rels[-1] > 1:                   # (a < b < c) - řetězení uvnitř
                    return i, False
                i += 1
                depth -= 1
                inside.pop()
                rels.pop()
            else:
                break                               # i LPAREN po operandu → konec (volání ve výrazu neexistuje)
    if expecting != "operator" or depth != 0 or any(r > 1 for r in rels):
        return i, False
    return i, True

def parse(stream):
    toks = []
    for t in stream.split():
        if t == "RPAREN!":
            toks.append(("RPAREN", True))
        else:
            toks.append((t, False))
    toks.append(("EOF", False))
    i, stack = 0, ["Program"]
    while stack:
        top = stack.pop()
        tok, flag = toks[i]
        if top in PSEUDO:
            i, ok = greedy(toks, i, top)
            if not ok:
                return False, f"neúplný výraz (visící operátor / závorka / řetězení relací) @[{i}]"
            continue
        if top in TERMS:
            if top == tok:
                i += 1
            else:
                return False, f"očekáváno {top}, přišlo {tok} @[{i}]"
            continue
        prod = table.get((top, tok))
        if prod is None:
            return False, f"prázdná buňka: {top} × {tok} @[{i}]"
        if (top in ("AfterCommaParam", "AfterCommaArg") and len(prod) == 0
                and tok == "RPAREN" and not flag):
            return False, f"koncová čárka bez EOL před ) @[{i}]"
        stack.extend(reversed(prod))
    if i == len(toks):
        return True, "OK"
    return False, "zbytky: " + " ".join(t[0] for t in toks[i:])

# ---------------------------- testy ----------------------------
W = ("IMPORT ID DOT MUL EOL DEF ID LPAREN RPAREN ASSIGN EOL INDENT {} DEDENT")

TESTS = [
    ("A: příkazy na samostatných řádcích", W.format("VAL ID ASSIGN INT_NUM EOL VAL ID ASSIGN INT_NUM EOL"), True),
    ("B: středník + další příkaz (učitelův mail)", W.format("VAL ID ASSIGN INT_NUM SEMICOLON VAL ID ASSIGN INT_NUM EOL"), True),
    ("C: středník, EOL, konec bloku", W.format("VAL ID ASSIGN INT_NUM SEMICOLON EOL"), True),
    ("D: vnořené bloky (while v if)", W.format("IF ID THEN EOL INDENT WHILE ID DO EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT DEDENT ELSE EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), True),
    ("E: běžný konec bloku", W.format("VAL ID ASSIGN INT_NUM EOL"), True),
    ("F: chybějící oddělovač → chyba 2", W.format("VAL ID ASSIGN INT_NUM VAL ID ASSIGN INT_NUM EOL"), False),
    ("G: DEDENT ELSE", W.format("IF ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), True),
    ("prázdné řádky v bloku", W.format("VAL ID ASSIGN INT_NUM EOL EOL EOL VAL ID ASSIGN INT_NUM EOL"), True),
    ("prázdný řádek před DEDENT", W.format("VAL ID ASSIGN INT_NUM EOL EOL"), True),
    ("středník + EOL + příkaz", W.format("VAL ID ASSIGN INT_NUM SEMICOLON EOL VAL ID ASSIGN INT_NUM EOL"), True),
    ("return se středníkem", W.format("RETURN INT_NUM SEMICOLON RETURN EOL"), True),
    ("return s voláním (strings.scala:12,37 subStr)", W.format("RETURN ID LPAREN ID COMMA INT_NUM COMMA ID RPAREN EOL"), True),
    ("return volání, EndSimple = ;", W.format("RETURN ID LPAREN ID RPAREN SEMICOLON"), True),
    ("return a + f(1) → chyba 2 (volání ve výrazu)", W.format("RETURN ID PLUS ID LPAREN INT_NUM RPAREN EOL"), False),
    ("return f(1) + 2 → chyba 2", W.format("RETURN ID LPAREN INT_NUM RPAREN PLUS INT_NUM EOL"), False),
    ("volání v podmínce → chyba 2 (FUNEXP až rozšíření)", W.format("IF ID LPAREN INT_NUM RPAREN THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), False),
    ("koncová čárka v parametrech (EOL před ) )", "IMPORT ID DOT MUL EOL DEF ID LPAREN ID COLON INT_TOKEN COMMA RPAREN! COLON INT_TOKEN ASSIGN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT", True),
    ("koncová čárka v parametrech na jednom řádku → chyba 2", "IMPORT ID DOT MUL EOL DEF ID LPAREN ID COLON INT_TOKEN COMMA RPAREN COLON INT_TOKEN ASSIGN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT", False),
    ("koncová čárka v argumentech (EOL před ) )", W.format("ID LPAREN INT_NUM COMMA RPAREN! EOL"), True),
    ("koncová čárka v argumentech na jednom řádku → chyba 2", W.format("ID LPAREN INT_NUM COMMA RPAREN EOL"), False),
    ("více parametrů", "IMPORT ID DOT MUL EOL DEF ID LPAREN ID COLON INT_TOKEN COMMA ID COLON DOUBLE_TOKEN RPAREN ASSIGN INT_NUM EOL", True),
    ("val s typem na stejném řádku", W.format("VAL ID COLON INT_TOKEN ASSIGN INT_NUM EOL"), True),
    ("val: typ na řádku, = na dalším → chyba 2 (v4, §4.1)", W.format("VAL ID COLON INT_TOKEN EOL ASSIGN INT_NUM EOL"), False),
    ("val: typ, prázdný řádek, = → chyba 2 (scanner EOLy slučuje)", W.format("VAL ID COLON INT_TOKEN EOL EOL ASSIGN INT_NUM EOL"), False),
    ("val: typ na řádku, = hlouběji odsazen → chyba 2 (skutečný stream s INDENT, v6)", W.format("VAL ID COLON INT_TOKEN EOL INDENT ASSIGN INT_NUM EOL DEDENT"), False),
    ("val = a výraz na dalším řádku → chyba 2 (v4)", W.format("VAL ID ASSIGN EOL INDENT INT_NUM EOL DEDENT"), False),
    ("val bez typu", W.format("VAL ID ASSIGN INT_NUM EOL"), True),
    ("var bez typu", W.format("VAR ID ASSIGN INT_NUM EOL"), True),
    ("var s typem na stejném řádku", W.format("VAR ID COLON INT_TOKEN ASSIGN INT_NUM EOL"), True),
    ("var bez inicializace → chyba 2", W.format("VAR ID COLON INT_TOKEN EOL"), False),
    ("přiřazení", W.format("ID ASSIGN ID PLUS INT_NUM EOL"), True),
    ("přiřazení volání", W.format("ID ASSIGN ID LPAREN INT_NUM RPAREN EOL"), True),
    ("volací příkaz", W.format("ID LPAREN ID RPAREN EOL"), True),
    ("volání s více argumenty", W.format("ID LPAREN ID COMMA INT_NUM COMMA DOUBLE_NUM RPAREN EOL"), True),
    ("výrazový příkaz od id", W.format("ID PLUS INT_NUM EOL"), True),
    ("samotné id jako výrazový příkaz", W.format("ID EOL"), True),
    ("výraz od literálu se závorkami", W.format("INT_NUM MUL LPAREN ID PLUS INT_NUM RPAREN EOL"), True),
    ("unární minus", W.format("MINUS ID PLUS INT_NUM EOL"), True),
    ("() jako příkaz (Unit)", W.format("LPAREN RPAREN EOL"), True),
    ("() jako jednořádkové tělo", "IMPORT ID DOT MUL EOL DEF ID LPAREN RPAREN COLON UNIT_TOKEN ASSIGN LPAREN RPAREN EOL", True),
    ("volání uvnitř výrazu → chyba 2", W.format("ID ASSIGN ID PLUS ID LPAREN INT_NUM RPAREN EOL"), False),
    ("x = f(1) + 2 → chyba 2", W.format("ID ASSIGN ID LPAREN INT_NUM RPAREN PLUS INT_NUM EOL"), False),
    ("x = y = 1 → chyba 2", W.format("ID ASSIGN ID ASSIGN INT_NUM EOL"), False),
    ("if bez else → chyba 2", W.format("IF ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), False),
    ("chybějící INDENT po then → chyba 2", W.format("IF ID THEN EOL VAL ID ASSIGN INT_NUM EOL"), False),
    ("podmínka v závorkách", W.format("IF LPAREN ID RPAREN THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), True),
    ("while", W.format("WHILE ID LE INT_NUM DO EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), True),
    ("return s výrazem", W.format("RETURN INT_NUM EOL"), True),
    ("holé return (otevřená otázka č. 1, sonda q1)", W.format("RETURN EOL"), True),
    ("jednořádkové tělo = volání", "IMPORT ID DOT MUL EOL DEF ID LPAREN RPAREN ASSIGN ID LPAREN RPAREN EOL", True),
    ("@main s blokem", "IMPORT ID DOT MUL EOL AT ID DEF ID LPAREN RPAREN COLON UNIT_TOKEN ASSIGN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT", True),
    ("prázdné řádky mezi definicemi", "IMPORT ID DOT MUL EOL DEF ID LPAREN RPAREN ASSIGN INT_NUM EOL EOL DEF ID LPAREN RPAREN ASSIGN INT_NUM EOL", True),
    ("prázdné řádky po importu", "IMPORT ID DOT MUL EOL EOL EOL DEF ID LPAREN RPAREN ASSIGN INT_NUM EOL", True),
    # v6: test „prázdný řádek mezi = a INDENT → chyba 2" odstraněn - scanner EOLy
    # slučuje, stream EOL EOL INDENT nikdy nenastane; jazykově tvar tolerujeme
    # (rozhodnutí §11.4 grammar.md).
    ("žádné definice (jen import)", "IMPORT ID DOT MUL EOL", True),
    ("příkaz na nejvyšší úrovni → chyba 2", "IMPORT ID DOT MUL EOL DEF ID LPAREN RPAREN ASSIGN INT_NUM EOL VAL ID ASSIGN INT_NUM EOL", False),
    ("faktoriál (if/return, val+volání v else)", "IMPORT ID DOT MUL EOL DEF ID LPAREN ID COLON INT_TOKEN RPAREN COLON INT_TOKEN ASSIGN EOL INDENT IF ID LE INT_NUM THEN EOL INDENT RETURN INT_NUM EOL DEDENT ELSE EOL INDENT VAL ID ASSIGN ID LPAREN ID RPAREN EOL RETURN ID MUL ID EOL DEDENT DEDENT", True),
    ("celý program (add + @main)", "IMPORT ID DOT MUL EOL DEF ID LPAREN ID COLON INT_TOKEN COMMA ID COLON INT_TOKEN RPAREN COLON INT_TOKEN ASSIGN EOL INDENT ID PLUS ID EOL DEDENT AT ID DEF ID LPAREN RPAREN COLON UNIT_TOKEN ASSIGN EOL INDENT VAL ID ASSIGN ID LPAREN INT_NUM COMMA INT_NUM RPAREN EOL ID LPAREN ID RPAREN EOL DEDENT", True),
    # v7 - originální specifikace PDF (v přepisu chybělo)
    ("@main na samostatném řádku (§2) → OK", "IMPORT ID DOT MUL EOL AT ID EOL DEF ID LPAREN RPAREN COLON UNIT_TOKEN ASSIGN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT", True),
    ("print(()) - () jako term v argumentu (§1.1, §2.3) → OK", W.format("ID LPAREN LPAREN RPAREN RPAREN EOL"), True),
    ("val x = 1 + (visící operátor) → chyba 2", W.format("VAL ID ASSIGN INT_NUM PLUS EOL"), False),
    ("val x = (1 + 2 (nezavřená závorka) → chyba 2", W.format("VAL ID ASSIGN LPAREN INT_NUM PLUS INT_NUM EOL"), False),
    ("val x = (1 +) (nedokončený obsah závorky) → chyba 2", W.format("VAL ID ASSIGN LPAREN INT_NUM PLUS RPAREN EOL"), False),
    ("if s prázdnou podmínkou → chyba 2", W.format("IF THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), False),
    ("a < b < c (řetězení relací, §3) → chyba 2", W.format("VAL ID ASSIGN ID LT ID LT ID EOL"), False),
    ("(a < b < c) v závorce → chyba 2", W.format("VAL ID ASSIGN LPAREN ID LT ID LT ID RPAREN EOL"), False),
]

# ---------------------------- výstup ----------------------------
def fmt_seq(rhs):
    return " ".join(rhs) if rhs else "ε"

print("=" * 72)
print("1) NULLABLE / FIRST / FOLLOW")
print("=" * 72)
for a in sorted(NT):
    fst = ", ".join(sorted(first[a] - {"EPS"})) + (" ε" if "EPS" in first[a] else "")
    fol = ", ".join(sorted(follow[a]))
    print(f"{a:16} FIRST={{{fst}}}  FOLLOW={{{fol}}}")

print()
print("=" * 72)
print("2) LL tabulka (SELECT řádky)")
print("=" * 72)
for a in sorted(NT):
    rows = []
    for aa, r in P:
        if aa != a:
            continue
        sel, n = seq_first(r)
        sel = sel - {"EPS"}
        if n:
            sel = sel | follow[a]
        rows.append((sorted(sel), r))
    for sel, r in rows:
        print(f"{a:16} [{', '.join(sel)}] -> {fmt_seq(r)}")

print()
print("=" * 72)
print("3) Kontrola kolizí v LL tabulce")
print("=" * 72)
if collisions:
    for a, t, r1, r2 in collisions:
        print(f"KOLIZE: {a} × {t}: {fmt_seq(r1)} vs {fmt_seq(r2)}")
    raise SystemExit("Gramatika NENÍ LL!")
print("OK - 0 kolizí, gramatika je LL.")

print()
print("=" * 72)
print("4) Trasování streamů")
print("=" * 72)
passed = 0
for name, stream, expected in TESTS:
    ok, msg = parse(stream)
    good = ok == expected
    passed += good
    mark = "PASS" if good else "FAIL"
    print(f"[{mark}] {name:52} → {'OK' if ok else 'ERR: ' + msg}")
print()
print(f"Výsledek: {passed}/{len(TESTS)} testů podle očekávání.")
