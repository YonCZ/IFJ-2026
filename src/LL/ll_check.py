#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ll_check.py - mechanická verifikace gramatiky IFJ26
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
    ("FuncDefs",      ("ExtensionDef", "FuncDefs")),   # v9 EXTKW: extension blok mezi definicemi
    ("FuncDefs",      ()),
    ("FuncDef",       ("DEF", "ID", "Params", "RetType", "ASSIGN", "Body")),
    ("FuncDef",       ("AT", "ID", "OptEol", "DEF", "ID", "Params", "RetType", "ASSIGN", "Body")),
    # v9 EXTKW: extension (p: T) + odsazené def metody; tvar = ukázka §5.9 PDF
    ("ExtensionDef",  ("EXTENSION", "LPAREN", "Param", "RPAREN", "EOL", "INDENT", "ExtDefs", "DEDENT")),
    ("ExtDefs",       ("EOL", "ExtDefs")),
    ("ExtDefs",       ("FuncDef", "ExtDefs")),      # @main uvnitř = sémantika, ne tvar
    ("ExtDefs",       ()),
    # v7: anotace @main může být na samostatném řádku (§2 PDF) - OptEol = EOL | ε
    ("OptEol",        ("EOL",)),
    ("OptEol",        ()),
    # v9 PARAMLISTS: 0..n skupin parametrů - Params je nullable celek
    # (def answer = 42 má nula skupin); FOLLOW(Params) = {COLON, ASSIGN}
    ("Params",        ("ParamGroup", "Params")),
    ("Params",        ()),
    ("ParamGroup",    ("LPAREN", "ParamList", "RPAREN")),
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
    ("Type",          ("BOOLEAN_TOKEN",)),   # v8 BOOLTHEN
    ("Body",          ("Rhs", "EOL")),
    ("Body",          ("EOL", "INDENT", "Block", "DEDENT")),
    ("Block",         ("BlockItem", "StmtRest")),
    ("BlockItem",     ("SimpleStmt", "LineEnd")),
    ("BlockItem",     ("IfStmt",)),
    ("BlockItem",     ("WhileStmt",)),
    ("BlockItem",     ("ForStmt",)),
    ("StmtRest",      ("EOL", "StmtRest")),
    ("StmtRest",      ("BlockItem", "StmtRest")),
    ("StmtRest",      ()),
    # v9: §1.2 - logický řádek nemůže začínat operátorem, ALE za středníkem
    # (stejný řádek) jsou unární operátory povoleny. LL si nepamatuje, co
    # snědl ukončovač → LineEnd/MidItem/MidSimple. Trailing ; řeší MidItem→EOL
    # (ne ε - to by kontaminovalo FOLLOW stmt-starty a kolidovalo s MidSimple).
    ("LineEnd",       ("EOL",)),                       # konec logického řádku
    ("LineEnd",       ("SEMICOLON", "MidItem")),        # pokračování téhož řádku
    ("MidItem",       ("MidSimple", "LineEnd")),
    ("MidItem",       ("IfStmt",)),
    ("MidItem",       ("WhileStmt",)),
    ("MidItem",       ("ForStmt",)),
    ("MidItem",       ("EOL",)),                        # trailing ; - řádek končí
    ("MidSimple",     ("ValStmt",)),
    ("MidSimple",     ("VarStmt",)),
    ("MidSimple",     ("ReturnStmt",)),
    ("MidSimple",     ("ID", "StmtIdTail")),
    ("MidSimple",     ("ExprNoId",)),                   # unární povoleny - mid-line
    ("SimpleStmt",    ("ValStmt",)),
    ("SimpleStmt",    ("VarStmt",)),
    ("SimpleStmt",    ("ReturnStmt",)),
    ("SimpleStmt",    ("ID", "StmtIdTail")),
    ("SimpleStmt",    ("ExprStmt",)),   # v8.1: §1.2 - logický řádek nezačíná operátorem
    ("ValStmt",       ("VAL", "ID", "ValVarTail")),
    ("VarStmt",       ("VAR", "ID", "ValVarTail")),
    # v4: Newlines smazán - typ a = na jednom logickém řádku (§1.2, grammar.md §4.1)
    ("ValVarTail",    ("COLON", "Type", "ASSIGN", "Rhs")),
    ("ValVarTail",    ("ASSIGN", "Rhs")),
    # v11 CONTROL: jednořádková těla větví if (§5.3). EOL-asymetrie: bloková
    # větev končí DEDENT (nic za ní → IfTail ε), jednořádková EOL (terminátor
    # patří větvi) → IfElseOpt→EOL. else if = řetěz (IfStmt); else-jednořádek
    # nesmí být if-výraz (ExprNoIf) - jinak kolize s řetězem.
    ("IfStmt",        ("IF", "Expr", "THEN", "IfThen")),
    ("IfThen",        ("EOL", "INDENT", "Block", "DEDENT", "IfTail")),
    ("IfThen",        ("Rhs", "IfElseOpt")),
    ("IfTail",        ("ELSE", "IfElse")),
    ("IfTail",        ()),
    ("IfElseOpt",     ("ELSE", "IfElse")),
    ("IfElseOpt",     ("EOL",)),                        # bez else - BOOLTHEN 
    ("IfElse",        ("EOL", "INDENT", "Block", "DEDENT")),
    ("IfElse",        ("IfStmt",)),                      # else if - řetěz 
    ("IfElse",        ("ExprNoIf", "EOL")),
    ("WhileStmt",     ("WHILE", "Expr", "DO", "EOL", "INDENT", "Block", "DEDENT")),
    # v11 CONTROL: for s JEDNÍM generátorem a 0..n strážemi (§5.3). Dva tvary
    # hlavičky: jednořádková (for i <- 1 to 10 by 2 do) a víceřádková (generátor
    # a stráže na vlastních logických řádcích odsazené, do na úrovni for).
    ("ForStmt",       ("FOR", "ForHead")),
    ("ForHead",       ("EOL", "INDENT", "ForGen", "ForGuardRest", "DEDENT", "DO", "DoBody")),
    ("ForHead",       ("ForGenInline", "DO", "DoBody")),
    ("ForGen",        ("ID", "LARROW", "Range", "EOL")),
    ("ForGenInline",  ("ID", "LARROW", "Range")),
    ("ForGuardRest",  ("EOL", "ForGuardRest")),
    ("ForGuardRest",  ("IF", "Expr", "EOL", "ForGuardRest")),
    ("ForGuardRest",  ()),
    ("DoBody",        ("EOL", "INDENT", "Block", "DEDENT")),
    ("DoBody",        ("Rhs", "EOL")),                   # jednořádkové tělo (jako u if)
    # Range: v modelu TO/UNTIL/BY tokeny; v reálném parseru ID + strcmp
    # (technika ifj26/main, fórum „Klíčová slova pri rozšíření CONTROL").
    # v11.1: by jen u inkluzivního tvaru (to ... by) - §5.3 výslovně
    # uvádí krok pouze u "from to to by step"; until bez by
    ("Range",         ("Expr", "RangeTail")),
    ("RangeTail",     ("TO", "Expr", "RangeBy")),
    ("RangeTail",     ("UNTIL", "Expr")),
    ("RangeBy",       ("BY", "Expr")),
    ("RangeBy",       ()),
    # v5: return umí i volání (strings.scala) - Rhs místo Expr; ε = holé return (otázka 1)
    ("ReturnStmt",    ("RETURN", "RhsOpt")),
    ("RhsOpt",        ("Rhs",)),
    ("RhsOpt",        ()),
    # v10 FUNEXP: volání je postfix na libovolném operandu → patří VÝRAZOVÉ
    # analýze. IdTail, ArgGroups, Term i pravidla Rhs mizí; Rhs se stává
    # aliasem (pseudoterminálem) pro Expr - FIRST beze změny.
    ("StmtIdTail",    ("ASSIGN", "Rhs")),
    ("StmtIdTail",    ("ExprFromId",)),      # v10: výrazový příkaz od uloženého ID
]
# v10 FUNEXP: argumenty jsou VÝRAZY (§5.1) - seznam argumentů zpracovává
# VÝRAZOVÁ analýza (zde _args(), v parseru sdílený parse_arg_list s metodami;
# koncová čárka jen s EOL-flagem). Pravidla ArgList/ArgRest/AfterCommaArg
# z P jsou proto pryč: byla by odpojená od Program (prázdné FOLLOW → tabulka
# bez buněk na RPAREN). Jazyk seznamu dokumentuje grammar.md §6.

NT = {a for a, _ in P}

E0 = {"ID", "INT_NUM", "DOUBLE_NUM", "STRING_LITERAL", "LPAREN", "PLUS", "MINUS"}
B0 = {"TRUE_TOKEN", "FALSE_TOKEN", "NOT_TOKEN"}   # v8 BOOLTHEN: literály + unární !
# interface pseudoterminály - mají FIRST, ale nemají vlastní pravidla
PSEUDO = {
    "Expr":        set(E0) | B0 | {"IF"},      # v11: + if-výraz (CONTROL §5.3)
    "Rhs":         set(E0) | B0 | {"IF"},      # v10: alias pro Expr (FUNEXP)
    "ExprNoIf":    set(E0) | B0,               # v11: else-větev-jednořádek bez if-výrazu
    "ExprNoId":    (set(E0) | B0) - {"ID"},    # totéž, volající zaručuje začátek != ID
    "ExprStmt":    (set(E0) | B0) - {"ID", "PLUS", "MINUS", "NOT_TOKEN"},
                                              # v8.1 §1.2: výrazový PŘÍKAZ nezačíná unárním
                                              # operátorem (logický řádek nezačíná operátorem)
    "ExprFromId":  {"PLUS", "MINUS", "MUL", "DIV", "LT", "LE", "GT", "GE",
                    "EQ", "NEQ", "EOL", "SEMICOLON",   # od uloženého ID; FOLLOW = {EOL,SEMI}
                    "AND_TOKEN", "OR_TOKEN",   # v8: && || pokračují od uloženého ID
                    # v9 VALMEM/EXTKW: postfix .člen patří výrazové analýze
                    "DOT",
                    "LPAREN"},                   # v10 FUNEXP: f(1) od uloženého ID = volání
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
OPS = {"PLUS", "MINUS", "MUL", "DIV", "LT", "LE", "GT", "GE", "EQ", "NEQ",
       "AND_TOKEN", "OR_TOKEN"}        # v8: && || - binární, řetězitelné (ne RELOPS)
RELOPS = {"LT", "LE", "GT", "GE", "EQ", "NEQ"}
OPERANDS = {"ID", "INT_NUM", "DOUBLE_NUM", "STRING_LITERAL",
             "TRUE_TOKEN", "FALSE_TOKEN"}    # v8: Bool literály jako operandy

def _args(toks, i):
    """v10 FUNEXP: LPAREN ArgList RPAREN uvnitř výrazu - argumenty jsou VÝRAZY
    (§5.1: „Argumentem může být libovolný výraz“). Pro každý argument se volá
    greedy() (výrazová analýza se rekurzivně volá sama - f(g(1), h(2) + 3)).
    Koncová čárka jen s EOL-flagem (RPAREN!) jako dřív; flag se hlídá na
    závěrečné ) seznamu, ne na vnořených. V reálném parseru SDÍLENÝ
    parse_arg_list (IdTail už neexistuje - volání i metody jednou cestou).
    Vrací index za RPAREN, nebo None = syntaktická chyba."""
    i += 1                                    # LPAREN
    if toks[i][0] == "RPAREN":                # prázdný seznam argumentů
        return i + 1
    while True:
        i, ok = greedy(toks, i, "Expr")       # argument = výraz (rekurze)
        if not ok:
            return None
        t = toks[i][0]
        if t == "RPAREN":
            return i + 1
        if t != "COMMA":
            return None
        i += 1                                # za čárkou
        if toks[i][0] == "RPAREN":            # koncová čárka - jen s EOL-flagem
            return i + 1 if toks[i][1] else None


def _if_branch(toks, i):
    """v11 CONTROL: větev if-výrazu za THEN/ELSE. Jednořádek = greedy
    (then se zastaví na ELSE, else na stopping tokenu kontextu) nebo
    break-form = EOL INDENT Block DEDENT - plný blok i s příkazy (v11.1:
    i s příkazy, §5.3; hodnotu větve určuje poslední příkaz, §2.3).
    Blok zpracovává syntaktická analýza (_run s ["Block"]) - kontrakt:
    výrazová analýza volá statement parser pro blokové větve.
    Vrací (i, ok)."""
    if toks[i][0] == "EOL":                       # break-form (§5.3: abs2/abs3)
        if i + 1 < len(toks) and toks[i + 1][0] == "INDENT":
            i, ok = _run(toks, i + 2, ["Block"])  # blok = i s příkazy (v11.1)
            if ok and i < len(toks) and toks[i][0] == "DEDENT":
                return i + 1, True
        return i, False
    i, ok = greedy(toks, i, "Expr")               # jednořáková větev
    return i, ok


def _if_expr(toks, i):
    """v11 CONTROL: if-výraz - IF už snědl volající (operand-stav greedy).
    IF cond THEN větev ELSE větev - ve výrazové rovině je else POVINNÉ
    (§5.3). Vnořený else-if rekurzí přes greedy (IF v operand-stavu).
    Vrací (i, ok)."""
    i, ok = greedy(toks, i, "Expr")                # podmínka - zastaví se na THEN
    if not ok or toks[i][0] != "THEN":
        return i, False
    i += 1
    i, ok = _if_branch(toks, i)                    # then-větev
    if not ok:
        return i, False
    if toks[i][0] != "ELSE":                       # výraz: else povinný
        return i, False
    i += 1
    i, ok = _if_branch(toks, i)                    # else-větev
    return i, ok


def greedy(toks, i, kind):
    """Aproximace výrazové analýzy: konzumuje výraz, na stopping tokenu se
    zastaví BEZ konzumace. kind=ExprFromId → začíná ve stavu 'operátor'
    (ID už přečteno volajícím - je to operand). Vrací (index, platnost).
    v10 FUNEXP: after_operand (místo after_member) - LPAREN v operátor-stavu
    po LIBOVOLNÉM operandu = volání (f(1) + 2, (1 + 2)(3), 1(2) syntakticky).
    Přechody after_operand: operand/()/.člen/args/zavřená závorka → True;
    binární operátor → False; unární nemění; ExprFromId startuje na True."""
    depth = 0
    inside = [0]      # počet tokenů uvnitř úrovně závorek (detekce (1 +))
    rels = [0]        # relační operátory v řetězu (řetězení zakázáno, §3)
    after_operand = (kind == "ExprFromId")   # uložené ID je operand
    expecting = "operand" if kind != "ExprFromId" else "operator"
    while i < len(toks):
        t = toks[i][0]
        if expecting == "operand":
            if t == "IF":                        # v11: if-výraz = primární výraz
                i += 1
                i, ok = _if_expr(toks, i)
                if not ok:
                    return i, False
                inside[-1] += 1
                expecting = "operator"
                after_operand = True
            elif t in OPERANDS:
                i += 1
                inside[-1] += 1
                expecting = "operator"
                after_operand = True
            elif t in ("PLUS", "MINUS", "NOT_TOKEN"):   # unární + - !
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
                after_operand = True               # () je operand
            else:
                break
        else:                                       # čekáme operátor nebo konec
            if t in OPS:
                i += 1
                inside[-1] += 1
                if t in RELOPS:
                    rels[-1] += 1
                    if rels[-1] > 1:           # v8.1: řetězení relací v jednom řetězu
                        return i, False
                elif t in ("AND_TOKEN", "OR_TOKEN"):
                    rels[-1] = 0               # v8.1: && || uzavírají relační řetěz (§5.2)
                expecting = "operand"
                after_operand = False
            elif t == "DOT":                    # .člen - postfix VALMEM/EXTKW
                i += 1
                if i < len(toks) and toks[i][0] == "ID":
                    i += 1
                    inside[-1] += 2
                    after_operand = True
                else:
                    return i, False              # visící tečka - neúplný výraz
            elif t == "LPAREN" and after_operand: # v10: volání na libovolném operandu
                j = _args(toks, i)
                if j is None:
                    return i, False
                i = j                            # after_operand zůstává True (f(1)(2))
                inside[-1] += 2
            elif t == "RPAREN" and depth > 0:
                if rels[-1] > 1:                 # (a < b < c) - řetězení uvnitř
                    return i, False
                i += 1
                depth -= 1
                inside.pop()
                rels.pop()
                after_operand = True              # v10: zavřený výraz je operand
            else:
                break
    if expecting != "operator" or depth != 0 or any(r > 1 for r in rels):
        return i, False
    return i, True

def _run(toks, i, stack):
    """Tabulkový LL parser nad toks od indexu i se zadaným zásobníkem.
    Vrací (i, ok) - ok=False znamená syntaktickou chybu (index ukazuje
    na místo chyby). V11.1: používá ho i _if_branch pro blokové větve
    if-výrazu (kontrakt: výrazová analýza volá statement parser)."""
    while stack:
        top = stack.pop()
        tok, flag = toks[i]
        if top in PSEUDO:
            i, ok = greedy(toks, i, top)
            if not ok:
                return i, False
            # v11.1 BROKE: výraz skončil DEDENTem (break-form if-výrazu) →
            # nadřazený příkaz končí jako víceřádkový (jako if/while).
            # Terminátor (EOL/LineEnd na vrcholu) se vynechá, pokud aktuální
            # token NENÍ EOL (prázdný řádek se konzumuje normálně).
            # Pozor: rozhoduje POSLEDNÍ token výrazu, ne "někde uvnitř
            # byl break" (v11 bug - review). Dokumentovaná odchylka §16.3.
            if (i > 0 and toks[i - 1][0] == "DEDENT" and toks[i][0] != "EOL"
                    and stack and stack[-1] in ("EOL", "LineEnd")):
                stack.pop()
            continue
        if top in TERMS:
            if top == tok:
                i += 1
            else:
                return i, False
            continue
        prod = table.get((top, tok))
        if prod is None:
            return i, False
        if (top in ("AfterCommaParam",) and len(prod) == 0
                and tok == "RPAREN" and not flag):
            return i, False
        stack.extend(reversed(prod))
    return i, True


def parse(stream):
    toks = []
    for t in stream.split():
        if t == "RPAREN!":
            toks.append(("RPAREN", True))
        else:
            toks.append((t, False))
    toks.append(("EOF", False))
    i, ok = _run(toks, 0, ["Program"])
    if not ok:
        return False, f"syntaktická chyba @[{i}] ({toks[i][0]})"
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
    ("return volání, EndSimple = ; (realistický stream s EOL před DEDENT)", W.format("RETURN ID LPAREN ID RPAREN SEMICOLON EOL"), True),
    ("return a + f(1) → OK (v10 FUNEXP: volání ve výrazu)", W.format("RETURN ID PLUS ID LPAREN INT_NUM RPAREN EOL"), True),
    ("return f(1) + 2 → OK (v10 FUNEXP)", W.format("RETURN ID LPAREN INT_NUM RPAREN PLUS INT_NUM EOL"), True),
    ("volání v podmínce → OK (v10 FUNEXP: podmínka = výraz)", W.format("IF ID LPAREN INT_NUM RPAREN THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), True),
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
    ("unární minus ve výrazu (val x = -y + 1; §1.2: řádek nezačíná -)", W.format("VAL ID ASSIGN MINUS ID PLUS INT_NUM EOL"), True),
    ("() jako příkaz (Unit)", W.format("LPAREN RPAREN EOL"), True),
    ("() jako jednořádkové tělo", "IMPORT ID DOT MUL EOL DEF ID LPAREN RPAREN COLON UNIT_TOKEN ASSIGN LPAREN RPAREN EOL", True),
    ("volání uvnitř výrazu → OK (v10 FUNEXP)", W.format("ID ASSIGN ID PLUS ID LPAREN INT_NUM RPAREN EOL"), True),
    ("x = f(1) + 2 → OK (v10 FUNEXP)", W.format("ID ASSIGN ID LPAREN INT_NUM RPAREN PLUS INT_NUM EOL"), True),
    ("x = y = 1 → chyba 2", W.format("ID ASSIGN ID ASSIGN INT_NUM EOL"), False),
    ("if bez else - v základu chyba 2, od v8 (BOOLTHEN) legální", W.format("IF ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), True),
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
    # ---- v8 BOOLTHEN (spec §5.2) ----
    ("BOOL: val s typem Boolean", W.format("VAL ID COLON BOOLEAN_TOKEN ASSIGN TRUE_TOKEN EOL"), True),
    ("BOOL: val = true && x || false", W.format("VAL ID ASSIGN TRUE_TOKEN AND_TOKEN ID OR_TOKEN FALSE_TOKEN EOL"), True),
    ("BOOL: x = !enabled (unární !)", W.format("ID ASSIGN NOT_TOKEN ID EOL"), True),
    ("BOOL: !value jako jednořádkové tělo (negate)", "IMPORT ID DOT MUL EOL DEF ID LPAREN ID COLON BOOLEAN_TOKEN RPAREN COLON BOOLEAN_TOKEN ASSIGN NOT_TOKEN ID EOL", True),
    ("BOOL: if bez else + další příkaz", W.format("IF ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT VAL ID ASSIGN INT_NUM EOL"), True),
    ("BOOL: if bez else poslední v bloku, ELSE patří vnějšímu (dangling else)", W.format("IF ID THEN EOL INDENT IF ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT DEDENT ELSE EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), True),
    ("BOOL: else if řetězení (if / else if / else-blok)", W.format("IF ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE IF ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), True),
    ("BOOL: dvojité else if zakončené else-blokem", "IMPORT ID DOT MUL EOL DEF ID LPAREN RPAREN ASSIGN EOL INDENT IF ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE IF ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE IF ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT DEDENT", True),
    ("BOOL: else if bez závěrečného else", W.format("IF ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE IF ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), True),
    ("BOOL: podmínka true (literál)", W.format("IF TRUE_TOKEN THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), True),
    ("BOOL: podmínka !enabled", W.format("IF NOT_TOKEN ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), True),
    ("BOOL: negace && v podmínce", W.format("IF ID AND_TOKEN NOT_TOKEN ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), True),
    ("BOOL: negate(true) - Bool literál jako argument", W.format("ID LPAREN TRUE_TOKEN RPAREN EOL"), True),
    ("BOOL: readBoolean() (vestavěná = běžné volání)", W.format("VAL ID ASSIGN ID LPAREN RPAREN EOL"), True),
    ("BOOL: return s boolem", W.format("RETURN FALSE_TOKEN EOL"), True),
    ("BOOL: else bez ničeho → chyba 2", W.format("IF ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE"), False),
    ("BOOL: && na začátku příkazu → chyba 2 (nikdy nezačíná výraz)", W.format("AND_TOKEN ID EOL"), False),
    ("BOOL: visící && na konci → neúplný výraz", W.format("VAL ID ASSIGN TRUE_TOKEN AND_TOKEN EOL"), False),
    ("BOOL: visící + v přiřazení → neúplný výraz", W.format("ID ASSIGN ID PLUS EOL"), False),
    ("BOOL: true není typ → chyba 2", W.format("VAL ID COLON TRUE_TOKEN ASSIGN INT_NUM EOL"), False),
    ("BOOL: while stále NEMÁ else → chyba 2", W.format("WHILE ID DO EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), False),
    # ---- v8.1 fixes ----
    ("FIX: if/else/else (dvojitý else) → chyba 2", W.format("IF ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), False),
    ("FIX: if/else-blok + else if → chyba 2 (řetěz jen přes else if)", W.format("IF ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE IF ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), False),
    ("FIX: 1 < 2 && 3 < 4 (relační nad &&, §5.2) → OK", W.format("VAL ID ASSIGN INT_NUM LT INT_NUM AND_TOKEN INT_NUM LT INT_NUM EOL"), True),
    ("FIX: 1 < 2 < 3 && true (řetězení v jednom řetězu) → chyba 2", W.format("VAL ID ASSIGN INT_NUM LT INT_NUM LT INT_NUM AND_TOKEN TRUE_TOKEN EOL"), False),
    ("FIX: a < b && c < d || e < f (spojky oddělují řetězy) → OK", W.format("VAL ID ASSIGN ID LT ID AND_TOKEN ID LT ID OR_TOKEN ID LT ID EOL"), True),
    ("FIX: !true jako samostatný řádek → chyba 2 (§1.2)", W.format("NOT_TOKEN TRUE_TOKEN EOL"), False),
    ("FIX: -5 jako samostatný řádek → chyba 2 (§1.2, mezera z v7)", W.format("MINUS INT_NUM EOL"), False),
    ("FIX: kontrola - val x = !true stále OK (mid-line)", W.format("VAL ID ASSIGN NOT_TOKEN TRUE_TOKEN EOL"), True),
    ("FIX: kontrola - if !x then stále OK", W.format("IF NOT_TOKEN ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), True),
    # ---- v9: PARAMLISTS + VALMEM + EXTKW (spec §5.4/§5.9/§5.10) ----
    ("PL: def bez seznamu parametrů (def answer = 42)", "IMPORT ID DOT MUL EOL DEF ID ASSIGN INT_NUM EOL", True),
    ("PL: dvě skupiny (def add(a: Int)(b: Int) = a + b)", "IMPORT ID DOT MUL EOL DEF ID LPAREN ID COLON INT_TOKEN RPAREN LPAREN ID COLON INT_TOKEN RPAREN ASSIGN ID PLUS ID EOL", True),
    ("PL: prázdná + neprázdná skupina (def f()(x: Int) = x)", "IMPORT ID DOT MUL EOL DEF ID LPAREN RPAREN LPAREN ID COLON INT_TOKEN RPAREN ASSIGN ID EOL", True),
    ("PL: tři skupiny (def f()()(x: Int) = 1)", "IMPORT ID DOT MUL EOL DEF ID LPAREN RPAREN LPAREN RPAREN LPAREN ID COLON INT_TOKEN RPAREN ASSIGN INT_NUM EOL", True),
    ("PL: koncová čárka v druhé skupině (EOL před ))", "IMPORT ID DOT MUL EOL DEF ID LPAREN ID COLON INT_TOKEN RPAREN LPAREN ID COLON INT_TOKEN COMMA RPAREN! COLON INT_TOKEN ASSIGN INT_NUM EOL", True),
    ("PL: val x = add(1)(2) - volání se dvěma skupinami", W.format("VAL ID ASSIGN ID LPAREN INT_NUM RPAREN LPAREN INT_NUM RPAREN EOL"), True),
    ("PL: volací příkaz add(1)(2)", W.format("ID LPAREN INT_NUM RPAREN LPAREN INT_NUM RPAREN EOL"), True),
    ("PL: add(1)(2) + 3 → OK (v10 FUNEXP: řetěz volání ve výrazu)", W.format("VAL ID ASSIGN ID LPAREN INT_NUM RPAREN LPAREN INT_NUM RPAREN PLUS INT_NUM EOL"), True),
    ("PL: val x = answer - 0-skupinová fce = syntakticky id (dispatch symtable)", W.format("VAL ID ASSIGN ID EOL"), True),
    ("PL: answer + 1 - 0-skupinová fce ve výrazu (PDF: i bez FUNEXP)", W.format("VAL ID ASSIGN ID PLUS INT_NUM EOL"), True),
    ("PL: @main s nula skupinami (syntakticky OK; sémantika přijímá - 0 skupin = bez parametrů)", "IMPORT ID DOT MUL EOL AT ID DEF ID COLON UNIT_TOKEN ASSIGN EOL INDENT LPAREN RPAREN EOL DEDENT", True),
    ("VM: val s = x.toString", W.format("VAL ID ASSIGN ID DOT ID EOL"), True),
    ("VM: literál.člen (\"abc\".length)", W.format("VAL ID ASSIGN STRING_LITERAL DOT ID EOL"), True),
    ("VM: řetěz členů x.toString.length", W.format("VAL ID ASSIGN ID DOT ID DOT ID EOL"), True),
    ("VM: (a + b).toString - člen na závorce", W.format("VAL ID ASSIGN LPAREN ID PLUS ID RPAREN DOT ID EOL"), True),
    ("VM: x.toString jako příkaz", W.format("ID DOT ID EOL"), True),
    ("VM: .toString na začátku RHS → chyba 2 (tečka nezačíná výraz)", W.format("VAL ID ASSIGN DOT ID EOL"), False),
    ("VM: x. visící tečka → neúplný výraz", W.format("VAL ID ASSIGN ID DOT EOL"), False),
    ("VM: f(1).toString → OK (v10 FUNEXP: postfix na volání)", W.format("VAL ID ASSIGN ID LPAREN INT_NUM RPAREN DOT ID EOL"), True),
    ("EXT: extension blok s jednou metodou", "IMPORT ID DOT MUL EOL EXTENSION LPAREN ID COLON INT_TOKEN RPAREN EOL INDENT DEF ID LPAREN ID COLON INT_TOKEN RPAREN COLON INT_TOKEN ASSIGN EOL INDENT ID PLUS ID EOL DEDENT DEDENT", True),
    ("EXT: extension se dvěma metodami a prázdným řádkem", "IMPORT ID DOT MUL EOL EXTENSION LPAREN ID COLON DOUBLE_TOKEN RPAREN EOL INDENT DEF ID LPAREN ID COLON DOUBLE_TOKEN RPAREN ASSIGN ID EOL EOL DEF ID LPAREN RPAREN ASSIGN INT_NUM EOL DEDENT", True),
    ("EXT: val y = x.plus(2)", W.format("VAL ID ASSIGN ID DOT ID LPAREN INT_NUM RPAREN EOL"), True),
    ("EXT: x.plus(2) jako příkaz", W.format("ID DOT ID LPAREN INT_NUM RPAREN EOL"), True),
    ("EXT: více skupin u metody (x.plus(1)(2))", W.format("VAL ID ASSIGN ID DOT ID LPAREN INT_NUM RPAREN LPAREN INT_NUM RPAREN EOL"), True),
    ("EXT: literál jako přijímač (1.plus(2))", W.format("VAL ID ASSIGN INT_NUM DOT ID LPAREN INT_NUM RPAREN EOL"), True),
    ("EXT: více argumentů metody (x.plus(2, 3))", W.format("VAL ID ASSIGN ID DOT ID LPAREN INT_NUM COMMA INT_NUM RPAREN EOL"), True),
    ("EXT: koncová čárka v args metody (EOL před ))", W.format("VAL ID ASSIGN ID DOT ID LPAREN INT_NUM COMMA RPAREN! EOL"), True),
    ("EXT: koncová čárka v args metody na jednom řádku → chyba 2", W.format("VAL ID ASSIGN ID DOT ID LPAREN INT_NUM COMMA RPAREN EOL"), False),
    ("EXT: extension bez INDENT → chyba 2", "IMPORT ID DOT MUL EOL EXTENSION LPAREN ID COLON INT_TOKEN RPAREN EOL DEF ID LPAREN RPAREN ASSIGN INT_NUM EOL", False),
    ("EXT: extension uvnitř funkce → chyba 2 (jen na vrcholu)", W.format("EXTENSION LPAREN ID COLON INT_TOKEN RPAREN EOL INDENT DEF ID LPAREN RPAREN ASSIGN INT_NUM EOL DEDENT"), False),
    ("EXT: prázdný extension blok - ExtDefs→ε projde (nedosažitelné ze scanneru)", "IMPORT ID DOT MUL EOL EXTENSION LPAREN ID COLON INT_TOKEN RPAREN EOL INDENT DEDENT", True),
    # ---- v9 fixes (externí review) ----
    ("FIX: print(1); !true - unární za středníkem OK (§1.2)", W.format("ID LPAREN INT_NUM RPAREN SEMICOLON NOT_TOKEN TRUE_TOKEN EOL"), True),
    ("FIX: val x = 1; -x - unární za středníkem OK", W.format("VAL ID ASSIGN INT_NUM SEMICOLON MINUS ID EOL"), True),
    ("FIX: val x = 1; -5; !flag - řetěz za ;", W.format("VAL ID ASSIGN INT_NUM SEMICOLON MINUS INT_NUM SEMICOLON NOT_TOKEN ID EOL"), True),
    ("FIX: 1.take(()) - () jako term v args metody", W.format("INT_NUM DOT ID LPAREN LPAREN RPAREN RPAREN EOL"), True),
    ("FIX: (\"x\".length)() → OK (v10 FUNEXP: volání na výsledku závorek)", W.format("VAL ID ASSIGN LPAREN STRING_LITERAL DOT ID RPAREN LPAREN RPAREN EOL"), True),
    ("FIX: kontrola - 1.m(2)(3) řetěz skupin metody stále OK", W.format("INT_NUM DOT ID LPAREN INT_NUM RPAREN LPAREN INT_NUM RPAREN EOL"), True),
    ("FIX: kontrola - !true na vlastním řádku stále chyba 2", W.format("NOT_TOKEN TRUE_TOKEN EOL"), False),
    # ---- v10 FUNEXP (spec §5.1) ----
    ("FX: val result = 2 * factorial(readInt() + 1) - ukázka ze spec", W.format("VAL ID ASSIGN INT_NUM MUL ID LPAREN ID LPAREN RPAREN PLUS INT_NUM RPAREN EOL"), True),
    ("FX: print(toStr(x + strLength(subStr(\"s\", 1, 3)))) - ukázka ze spec", W.format("ID LPAREN ID LPAREN ID PLUS ID LPAREN STRING_LITERAL COMMA INT_NUM COMMA INT_NUM RPAREN RPAREN RPAREN EOL"), True),
    ("FX: f(g(1), h(2) + 3) - výrazy jako argumenty", W.format("VAL ID ASSIGN ID LPAREN ID LPAREN INT_NUM RPAREN COMMA ID LPAREN INT_NUM RPAREN PLUS INT_NUM RPAREN EOL"), True),
    ("FX: f(1)(2)(3) - tři skupiny volání", W.format("VAL ID ASSIGN ID LPAREN INT_NUM RPAREN LPAREN INT_NUM RPAREN LPAREN INT_NUM RPAREN EOL"), True),
    ("FX: f(1).g(2).h(3) - mix volání a metod v řetězu", W.format("VAL ID ASSIGN ID LPAREN INT_NUM RPAREN DOT ID LPAREN INT_NUM RPAREN DOT ID LPAREN INT_NUM RPAREN EOL"), True),
    ("FX: (1 + 2)(3) - volání na výsledku závorek", W.format("VAL ID ASSIGN LPAREN INT_NUM PLUS INT_NUM RPAREN LPAREN INT_NUM RPAREN EOL"), True),
    ("FX: 1(2) - volání na literálu syntakticky OK (sémantika odmítne)", W.format("VAL ID ASSIGN INT_NUM LPAREN INT_NUM RPAREN EOL"), True),
    ("FX: print(1); f(2) - volání za středníkem", W.format("ID LPAREN INT_NUM RPAREN SEMICOLON ID LPAREN INT_NUM RPAREN EOL"), True),
    ("FX: f(g(), 1) - prázdné volání jako argument", W.format("VAL ID ASSIGN ID LPAREN ID LPAREN RPAREN COMMA INT_NUM RPAREN EOL"), True),
    ("FX: f(1,) s EOL-flagem - koncová čárka u výrazových args", W.format("ID LPAREN INT_NUM COMMA RPAREN! EOL"), True),
    ("FX: f(1,) bez EOL-flagu → chyba 2", W.format("ID LPAREN INT_NUM COMMA RPAREN EOL"), False),
    ("FX: kontrola - x = y = 1 stále chyba 2 (přiřazení není výraz)", W.format("ID ASSIGN ID ASSIGN INT_NUM EOL"), False),
    ("FX: kontrola - f(1 (nezavřené) → neúplný výraz", W.format("VAL ID ASSIGN ID LPAREN INT_NUM EOL"), False),
    # ---- v11 CONTROL (spec §5.3) ----
    ("CTRL: sign - if-výraz s vnořeným else-if (control.scala:7)", W.format("VAL ID ASSIGN IF ID LT INT_NUM THEN MINUS INT_NUM ELSE IF ID EQ INT_NUM THEN INT_NUM ELSE INT_NUM EOL"), True),
    ("CTRL: break-form obě větve (abs2)", W.format("VAL ID ASSIGN IF ID LT INT_NUM THEN EOL INDENT INT_NUM MINUS INT_NUM EOL DEDENT ELSE EOL INDENT ID EOL DEDENT"), True),
    ("CTRL: break-form jen else (abs3)", W.format("VAL ID ASSIGN IF ID LT INT_NUM THEN INT_NUM MINUS INT_NUM ELSE EOL INDENT ID EOL DEDENT"), True),
    ("CTRL: break-form, za ním další příkaz", W.format("VAL ID ASSIGN IF ID LT INT_NUM THEN EOL INDENT INT_NUM EOL DEDENT ELSE EOL INDENT ID EOL DEDENT VAL ID ASSIGN INT_NUM EOL"), True),
    ("CTRL: break-form jako poslední příkaz bloku", W.format("VAL ID ASSIGN IF ID THEN EOL INDENT INT_NUM EOL DEDENT ELSE EOL INDENT ID EOL DEDENT"), True),
    ("CTRL: if-příkaz, obě větve jednořájkové", W.format("IF ID THEN ID ELSE ID EOL"), True),
    ("CTRL: if c then x (bez else, jednořádek, BOOLTHEN)", W.format("IF ID THEN ID EOL"), True),
    ("CTRL: then blok + else jednořádek", W.format("IF ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE ID EOL"), True),
    ("CTRL: then jednořádek + else blok", W.format("IF ID THEN ID ELSE EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), True),
    ("CTRL: then blok + else if (řetěz za blokem)", W.format("IF ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT ELSE IF ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), True),
    ("CTRL: if za středníkem", W.format("ID LPAREN INT_NUM RPAREN SEMICOLON IF ID THEN ID EOL"), True),
    ("CTRL: if-výraz v závorce + aritmetika", W.format("VAL ID ASSIGN LPAREN IF ID THEN INT_NUM ELSE INT_NUM RPAREN PLUS INT_NUM EOL"), True),
    ("CTRL: if-výraz jako argument", W.format("ID LPAREN IF ID THEN INT_NUM ELSE INT_NUM RPAREN EOL"), True),
    ("CTRL: return if-výraz", W.format("RETURN IF ID THEN INT_NUM ELSE INT_NUM EOL"), True),
    ("CTRL: if-výraz v podmínce while", W.format("WHILE IF ID THEN TRUE_TOKEN ELSE FALSE_TOKEN DO EOL INDENT VAL ID ASSIGN INT_NUM EOL DEDENT"), True),
    ("CTRL: for jednořáková hlavička (to + by -2)", W.format("FOR ID LARROW INT_NUM TO INT_NUM BY MINUS INT_NUM DO ID LPAREN ID RPAREN EOL"), True),
    ("CTRL: for until jednořákové (bez by - §5.3)", W.format("FOR ID LARROW INT_NUM UNTIL INT_NUM DO ID LPAREN ID RPAREN EOL"), True),
    ("CTRL: for víceřáková hlavička (gen + 2 stráže, control.scala)", W.format("FOR EOL INDENT ID LARROW INT_NUM TO ID BY ID EOL IF ID NEQ INT_NUM EOL IF ID LT INT_NUM EOL DEDENT DO EOL INDENT ID ASSIGN ID PLUS ID EOL DEDENT"), True),
    ("CTRL: for výrazy v range", W.format("FOR ID LARROW ID PLUS ID TO ID MUL INT_NUM BY ID DO ID LPAREN ID RPAREN EOL"), True),
    ("CTRL: for s prázdným řádkem v hlavičce", W.format("FOR EOL INDENT ID LARROW INT_NUM TO INT_NUM EOL EOL IF ID LT INT_NUM EOL DEDENT DO EOL INDENT ID EOL DEDENT"), True),
    ("CTRL: for blokové tělo, vnořený for", W.format("FOR ID LARROW INT_NUM UNTIL INT_NUM DO EOL INDENT FOR ID LARROW INT_NUM TO INT_NUM DO ID LPAREN ID RPAREN EOL DEDENT"), True),
    ("CTRL: for jednořákové tělo = výraz", W.format("FOR ID LARROW INT_NUM TO INT_NUM DO ID PLUS ID EOL"), True),
    ("CTRL: celý control.scala", "IMPORT ID DOT MUL EOL AT ID DEF ID LPAREN RPAREN COLON UNIT_TOKEN ASSIGN EOL INDENT VAL ID ASSIGN INT_NUM EOL VAL ID ASSIGN INT_NUM EOL VAL ID ASSIGN IF ID LT INT_NUM THEN MINUS INT_NUM ELSE IF ID EQ INT_NUM THEN INT_NUM ELSE INT_NUM EOL VAR ID ASSIGN INT_NUM EOL FOR EOL INDENT ID LARROW INT_NUM TO ID BY ID EOL IF ID NEQ INT_NUM EOL IF ID LT INT_NUM EOL DEDENT DO EOL INDENT ID ASSIGN ID PLUS ID EOL DEDENT ID LPAREN STRING_LITERAL RPAREN EOL ID LPAREN ID RPAREN EOL ID LPAREN STRING_LITERAL RPAREN EOL ID LPAREN ID RPAREN EOL ID LPAREN STRING_LITERAL RPAREN EOL FOR ID LARROW INT_NUM TO INT_NUM BY MINUS INT_NUM DO EOL INDENT ID LPAREN STRING_LITERAL RPAREN EOL ID LPAREN ID RPAREN EOL DEDENT ID LPAREN STRING_LITERAL RPAREN EOL ID LPAREN STRING_LITERAL RPAREN EOL FOR ID LARROW INT_NUM UNTIL INT_NUM DO EOL INDENT ID LPAREN STRING_LITERAL RPAREN EOL ID LPAREN ID RPAREN EOL DEDENT ID LPAREN STRING_LITERAL RPAREN EOL DEDENT", True),
    ("CTRL NEG: if-výraz bez else", W.format("VAL ID ASSIGN IF ID THEN ID EOL"), False),
    ("CTRL NEG: for bez do", W.format("FOR ID LARROW INT_NUM TO INT_NUM EOL"), False),
    ("CTRL NEG: dva generátory", W.format("FOR EOL INDENT ID LARROW INT_NUM TO INT_NUM EOL ID LARROW INT_NUM TO INT_NUM EOL DEDENT DO EOL INDENT ID EOL DEDENT"), False),
    ("CTRL NEG: inline stráž (jen víceřáková hlavička)", W.format("FOR ID LARROW INT_NUM TO INT_NUM IF ID LT INT_NUM DO ID EOL"), False),
    ("CTRL NEG: for jako výraz", W.format("VAL ID ASSIGN FOR ID LARROW INT_NUM TO INT_NUM DO ID EOL"), False),
    ("CTRL NEG: i <- 1 bez to/until", W.format("FOR ID LARROW INT_NUM DO ID EOL"), False),
    ("CTRL NEG: while stále jen blok", W.format("WHILE ID DO ID EOL"), False),
    ("CTRL NEG: if c then a; b (jednořáková větev = jeden výraz)", W.format("IF ID THEN ID SEMICOLON ID EOL"), False),
    ("CTRL NEG: for tělo bez INDENT", W.format("FOR ID LARROW INT_NUM TO INT_NUM DO VAL ID ASSIGN INT_NUM EOL"), False),
    ("CTRL NEG: visící else (if c then x⏎else y - konzistentní se Scalou)", W.format("IF ID THEN ID EOL ELSE EOL INDENT ID EOL DEDENT"), False),
    # ---- v11.1 fixes (externí review) ----
    ("FIX: break-form větev = plný blok i s příkazy (val + výsledek)", W.format("VAL ID ASSIGN IF ID THEN EOL INDENT VAL ID ASSIGN INT_NUM EOL ID PLUS ID EOL DEDENT ELSE EOL INDENT ID EOL DEDENT"), True),
    ("FIX: BROKE - else-jednořádek po then-break-form: val y bez ; → chyba 2", W.format("VAL ID ASSIGN IF TRUE_TOKEN THEN EOL INDENT INT_NUM EOL DEDENT ELSE INT_NUM VAL ID ASSIGN INT_NUM EOL"), False),
    ("FIX: BROKE - vnořený if-výraz v break-formu, pokračování print(x)", W.format("VAL ID ASSIGN IF TRUE_TOKEN THEN INT_NUM ELSE IF FALSE_TOKEN THEN EOL INDENT INT_NUM EOL DEDENT ELSE EOL INDENT INT_NUM EOL DEDENT ID LPAREN ID RPAREN EOL"), True),
    ("FIX: until bez by (§5.3: krok jen u to)", W.format("FOR ID LARROW INT_NUM UNTIL INT_NUM BY INT_NUM DO ID EOL"), False),
    ("FIX: kontrola - to + by stále OK", W.format("FOR ID LARROW INT_NUM TO INT_NUM BY INT_NUM DO ID EOL"), True),
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
