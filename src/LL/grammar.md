# IFJ26 LL Gramatika

Základní zadání se sedmi rozšířeními: BOOLTHEN, PARAMLISTS, VALMEM, EXTKW,
FUNEXP, CONTROL, INTERPOLATEDS. Všechna jsou zapracovaná, ověřená
a zdokumentovaná v sekcích 13 až 17.

| rozšíření | co přidává | kde |
|---|---|---|
| BOOLTHEN | Boolean typ, literály, `!`, `&&`, `\|\|`, volitelné `else`, `else if` | §13 |
| PARAMLISTS | 0..n skupin parametrů, `def f = 42` | §14 |
| VALMEM | `.člen` postfix na hodnotách | §14 |
| EXTKW | `extension` bloky s metodami | §14 |
| FUNEXP | volání jako součást výrazu, výrazy jako argumenty | §15 |
| CONTROL | jednořádková těla if, if-výraz, for s generátory a strážemi | §16 |
| INTERPOLATEDS | interpolované řetězce `s"..."` a `s"""..."""` | §17 |

Verifikace: `ll_check.py` kontroluje LL bezkoliznost a tabulkovým
parserem prochází 236 tokenových streamů (236/236, §9). Výrazy řeší
precedenční tabulka, skript ověřuje jen strukturu příkazů.

## 0. Konvence

- Terminály = tokeny z `token.h`:
  `IMPORT DEF VAL VAR IF THEN ELSE WHILE DO RETURN ID INT_NUM DOUBLE_NUM STRING_LITERAL
  LPAREN RPAREN COMMA COLON SEMICOLON ASSIGN DOT MUL AT EOL INDENT DEDENT EOF
  INT_TOKEN DOUBLE_TOKEN STRING_TOKEN UNIT_TOKEN PLUS MINUS
  TRUE FALSE NOT AND OR BOOLEAN`
  (a operátory výrazů `MUL DIV LT LE GT GE EQ NEQ`, celé v kompetenci
  precedenční analýzy).
  Pozor: `INT_TOKEN` = typ, `INT_NUM` = literál, dva různé tokeny.
  Před přepisem do C mechanicky srovnat přesné identifikátory z `token.h`
  (např. `INT_NUM_TOKEN`, `STRING_LITERAL_TOKEN`).
- Neterminály CamelCase; v C funkce `parse_xxx` ve snake_case.
  Ocasy seznamů mají suffix `Rest` (`ParamRest`, `StmtRest`), celé seznamy
  plurál nebo `List` (`FuncDefs`, `ParamList`).
- `ε` = pravá strana prázdná. Komentáře `(* … *)`.
- `Expr`, `ExprNoId`, `ExprFromId` = interface pseudoterminály pro
  precedenční analýzu (§5): mají FIRST, ale nemají vlastní pravidla;
  v kódu to jsou volání její analýzy.

## 1. Nejvyšší patro

```ebnf
Program  → Prolog FuncDefs EOF
Prolog   → IMPORT ID DOT MUL EOL          (* v kódu: attr(ID)=="ifj26", jinak chyba 2 *)
FuncDefs → EOL FuncDefs                   (* prázdné řádky po importu / mezi definicemi *)
         | FuncDef FuncDefs
         | ExtensionDef FuncDefs           (*  EXTKW *)
         | ε                              (* Predikt: {EOL} | {DEF, AT} | {EXTENSION} | {EOF} *)
FuncDef  → DEF ID Params RetType ASSIGN Body
         | AT ID OptEol DEF ID Params RetType ASSIGN Body
                                        (* v kódu: attr(ID za AT)=="main" *)
OptEol   → EOL
         | ε                             (* Predikt: {EOL} | FOLLOW = {DEF} *)

ExtensionDef → EXTENSION LPAREN Param RPAREN EOL INDENT ExtDefs DEDENT
ExtDefs      → EOL ExtDefs              (*  EXTKW: metody rozšíření; tvar = §5.9 PDF *)
             | FuncDef ExtDefs          (* @main uvnitř = sémantika, ne tvar *)
             | ε                         (* Predikt: {EOL, AT, DEF} | FOLLOW = {DEDENT} *)
```

Řešení:

- Na vrchní úrovni jsou jen definice funkcí. Příkaz po definicích skončí
  chybou 2 (prázdná buňka `FuncDefs × VAL`).
- Prázdné řádky konzumuje seznam (`FuncDefs → EOL FuncDefs`). Podle
  specifikace §1.2 se ignorují, takže je řeší jediné pravidlo.
  Stejný vzor jako `StmtRest` v §3.
- `OptEol`: anotace @main může být na samostatném řádku i na stejném
  řádku s `def` (specifikace §2). Znak @ se v jiném kontextu
  nepoužívá, mezi ním a `main` nesmí být žádný znak.
- Kontrola `ifj26` a `main` probíhá v kódu pomocí `strcmp` na atributu
  tokenu. Gramatika neumí vyjádřit identifikátor s konkrétní hodnotou.

## 2. Hlavička funkce

```ebnf
Params          → ParamGroup Params           (* : 0..n skupin; def answer = 42 má nula *)
                | ε                            (* Predikt: {LPAREN} | FOLLOW = {COLON, ASSIGN} *)
ParamGroup      → LPAREN ParamList RPAREN
ParamList       → Param ParamRest
                | ε                              (* {ID} | {RPAREN} *)
ParamRest       → COMMA AfterCommaParam
                | ε                              (* {COMMA} | {RPAREN} *)
AfterCommaParam → Param ParamRest               (* {ID}: další parametr *)
                | ε                              (* {RPAREN} + v kódu EOL-flag, bez něj chyba 2 *)
Param           → ID COLON Type
RetType         → COLON Type                    (* {COLON} *)
                | ε                              (* FOLLOW = {ASSIGN} *)
Type            → INT_TOKEN | DOUBLE_TOKEN | STRING_TOKEN | UNIT_TOKEN
                | BOOLEAN_TOKEN                 (*  BOOLTHEN *)
Body            → Rhs EOL                       (* jednořádkové tělo *)
                | EOL INDENT Block DEDENT        (* blokové tělo *)
```

Řešení:

- Levá faktorizace: po `COMMA` rozhoduje `AfterCommaParam` jedním
  lookaheadem (`ID` = další parametr, `RPAREN` = konec seznamu).
  Dvě pravidla začínající stejným tokenem by kolidovala.
- Koncová čárka (§1.2): „Za poslední položkou neprázdného seznamu
  parametrů … je povolena koncová čárka, pouze pokud je následujícím
  tokenem uzavírací závorka a mezi čárkou a touto závorkou je znak
  konce řádku.“ Řeší ji větev `AfterCommaParam → ε`: `RPAREN` konzumuje až
  nadřazené `Params`, parser v ε-větvi ověří flag ze scanneru
  (konec řádku před touto `)`). Bez flagu jde o chybu 2.
- Návratový typ je syntakticky nepovinný (`RetType → ε`, FOLLOW
  = `{ASSIGN}`). Povinnost při rekurzi nebo výskytu `return` je
  sémantická kontrola (typová chyba 6).
- Tělo má dvě formy (§2.3). Rozlišuje je jeden lookahead: začátek
  výrazu nebo `EOL`.

## 3. Blok a seznam příkazů

Ukončovatele určují dvě kategorie příkazů:

| kategorie | příkazy | konec |
|---|---|---|
| jednořádkové | val, var, return, přiřazení, volání, výraz | povinný `LineEnd` (EOL nebo `;`) |
| víceřádkové | if, while | DEDENT svého bloku, nic extra za nimi |

```ebnf
Block      → BlockItem StmtRest              (* neprázdný, §2.1 *)
BlockItem  → SimpleStmt LineEnd
           | IfStmt
           | WhileStmt
           | ForStmt                          (* CONTROL *)
StmtRest   → EOL StmtRest                    (* prázdné řádky *)
           | BlockItem StmtRest               (* další příkaz, začátek řádku *)
           | ε                                 (* FOLLOW = {DEDENT} *)
LineEnd    → EOL                             (* konec logického řádku *)
           | SEMICOLON MidItem               (* pokračování téhož řádku *)
MidItem    → MidSimple LineEnd               (* ; pak další příkaz, bez bloku *)
           | IfStmt
           | WhileStmt
           | ForStmt                          (* inline forma, CONTROL *)
           | EOL                              (* trailing ; - řádek končí *)
MidSimple  → ValStmt | VarStmt | ReturnStmt | ID StmtIdTail
           | ExprNoId                         (* unární operátory povoleny *)
```

`MidSimple` je `SimpleStmt` s unárními operátory: §1.2 zakazuje operátor
pouze na začátku logického řádku - za středníkem na stejném řádku
je `print(1); !true` legální (externí review). Trailing `;`
(`val a = 1;` EOL) řeší `MidItem → EOL`, nikoli ε - ε by kontaminovalo
FOLLOW stmt-starty a kolidovalo s `MidSimple` (ověřeno dvěma spadlými
pokusy).

Řešení:

- Ukončovatel patří příkazu. Kdyby EOL konzumovalo okolí, bylo by
  přijato `val a = 1 val b = 2` bez oddělovače. S povinným `LineEnd`
  skončí chybou 2 (prázdná buňka `LineEnd × VAL`).
- Volné řádky konzumuje seznam. Za `;` může přijít EOL a před
  DEDENT mohou být prázdné řádky. Pravidlo `StmtRest → EOL StmtRest`
  konzumuje EOLy po jednom a vždy znovu rozhoduje.
- Blok je neprázdný (§2.1). Prázdné tělo se zapisuje literálem `()`.
- Víceřádkové příkazy nemají za sebou ukončovatel: EOL patří
  poslednímu příkazu uvnitř bloku, DEDENT zavírá konstrukci.

Rohové případy (ověřeno trasováním, §9):

| | stream | výsledek |
|---|---|---|
| A | `VAL ID ASSIGN INT EOL VAL …` | OK, `LineEnd=EOL`, další BlockItem |
| B | `… INT SEMI VAL …` | OK, `LineEnd → SEMI MidItem` (mid-line, unární OK) |
| C | `… INT SEMI EOL DEDENT` | OK, `StmtRest→EOL StmtRest→ε` |
| D | `… DEDENT DEDENT …` | OK, každý DEDENT patří své konstrukci |
| E | `… INT EOL DEDENT` | OK, `StmtRest→ε` |
| F | `… INT VAL …` | chyba 2, `LineEnd × VAL`, prázdná buňka |
| G | `… DEDENT ELSE …` | OK, `StmtRest→ε`, IfStmt pokračuje ELSE |

## 4. Příkazy

```ebnf
SimpleStmt → ValStmt | VarStmt | ReturnStmt
           | ID StmtIdTail                   (* přiřazení / volání / výraz od id *)
           | ExprStmt                        (* výrazový příkaz - bez unárních
                                              operátorů, §1.2: logický řádek
                                              nezačíná operátorem *)
ForStmt    → FOR ForHead                      (* CONTROL *)
ForHead    → EOL INDENT ForGen ForGuardRest DEDENT DO DoBody
           | ForGenInline DO DoBody           (* inline hlavička *)
ForGen     → ID LARROW Range EOL
ForGenInline → ID LARROW Range
ForGuardRest → EOL ForGuardRest               (* prázdné řádky *)
             | IF Expr EOL ForGuardRest       (* stráž *)
             | ε
DoBody     → EOL INDENT Block DEDENT          (* tělo = blok *)
           | Rhs LineEnd                     (* tělo = jednořádek; ; ukončí for *)
Range      → Expr RangeTail
RangeTail  → TO Expr RangeBy
           | UNTIL Expr
RangeBy    → BY Expr
           | ε
ValStmt    → VAL ID ValVarTail
VarStmt    → VAR ID ValVarTail
ValVarTail → COLON Type ASSIGN Rhs            (* s typem, jeden logický řádek *)
           | ASSIGN Rhs                       (* bez typu *)
IfStmt     → IF Expr THEN IfThen
IfThen     → EOL INDENT Block DEDENT IfTail   (* then = blok *)
           | Rhs IfElseOpt                    (* then = jednořádek *)
IfTail     → ELSE IfElse                      (* po blokové větvi *)
           | ε
IfElseOpt  → ELSE IfElse                      (* po jednořádkové větvi *)
           | LineEnd                          (* bez else - BOOLTHEN *)
IfElse     → EOL INDENT Block DEDENT          (* else = blok, ukončuje řetěz *)
           | IfStmt                           (* else if - řetěz *)
           | ExprNoIf LineEnd                 (* else = jednořádek; ; ukončí if *)
WhileStmt  → WHILE Expr DO EOL INDENT Block DEDENT
ReturnStmt → RETURN RhsOpt                   (* výraz i volání *)
RhsOpt     → Rhs
           | ε                                (* holé return, §11.1; FOLLOW = {EOL, SEMICOLON} *)
```

Řešení:

- Kolize `id` na úrovni příkazu je čistě syntaktická. Za `id`
  rozhoduje jeden lookahead: `=` znamená přiřazení, `(` volání,
  ostatní tokeny výraz. Tabulka symbolů v této vrstvě není potřeba.
- `val` a `var` sdílejí `ValVarTail` (liší se prvním tokenem).
  Inicializace je vždy povinná (§2.2): `var x: Int` bez `=` skončí
  chybou 2 (prázdná buňka `ASSIGN × EOL`).
- `if` a `while`: po hlavičce následuje EOL a odsazený blok (§1.2).
  `else` je volitelný (BOOLTHEN, §13.3), `else if` je rekurzivní
  `IfStmt` (§13.4). Za `DEDENT` větve then následuje `ELSE` přímo.
- `return` přebírá výraz (`RhsOpt`). Volání je součást výrazu od FUNEXP (§15).

### 4.1 Typ a `=` na různých řádcích: chyba 2

`val c: Int⏎ = 3` gramatika odmítá. Zdůvodnění:

1. §1.2: logický řádek končí LF „mimo závorky, víceřádkové řetězce
   a blokové komentáře“. LF za `Int` není v žádné výjimce, takže
   `val c: Int` je celý logický řádek a je neúplný.
2. §2.2 ukazuje definici jako jeden logický řádek
   (`val id [: typ] = výraz-nebo-volání`). Víceřádkovost spec
   povoluje jen u závorek (§1.2).
3. §1: „přijetí programu překladačem Scala proto neznamená, že jde
   o platný program v IFJ26“. Referenční běh Scaly nemůže legalitu
   v IFJ26 dokázat, jen vyvrátit. Rozhoduje specifikace.
4. Asymetrie odsazení: pokračování se stejným odsazením by prošlo,
   hlubší by skončilo na INDENT (EOL scanner spolkne, INDENT ne).
   Chování by záviselo na stylu pokračování, který spec nikde
   nepopisuje.

Stejným §1.2 důvodem gramatika odmítá i `val c =⏎    3` (inicializace
na dalším řádku): blokovou formu spec ukazuje výslovně jen u `def`
(§2.3), u `val`/`var` nikoli. Dlouhé inicializace se v IFJ26 lámou
závorkami (uvnitř závorek LF nevadí).

## 5. Výraz: Rhs jako alias, dispatch na úrovni příkazu

```ebnf
Rhs        → Expr                           (* FUNEXP: alias - volání je
                                              postfix výrazu, styčný bod jako Expr *)
StmtIdTail → ASSIGN Rhs                      (* přiřazení, jen na úrovni příkazu *)
           | ExprFromId                      (* výrazový příkaz od uloženého ID;
                                              LPAREN = volání f(1)  *)

Expr       → (* výraz od aktuálního tokenu *)
ExprNoId   → (* totéž; volající zaručuje, že začátek není ID *)
ExprFromId → (* výraz od už přečteného tokenu ID - je to operand *)
```

`Rhs` je pseudoterminál (alias pro `Expr`, FIRST beze změny) -
`IdTail`, `ArgGroups` a `Term` mizí: volání je postfix
na libovolném operandu a patří výrazové analýze (§15).
`Expr`, `ExprNoId`, `ExprFromId`, `Rhs` nemají pravidla; jsou to styčné body
s precedenční analýzou. Výrazy jsou v LL gramatice záměrně
(účel: zadání výrazy přiděluje precedenční analýze, přirozená
gramatika výrazů používá levou rekurci, která je v LL zakázaná).
Jejich FIRST a stopping tokeny definuje `summary.md` (kontrakt s
výrazovou analýzou).

### 5.1 Dispatch úvodního ID podle symtable

Dispatch za úvodním `ID` v pozici `Rhs` nebo `StmtIdTail` dělá parser
podle symtable, ne čistě syntakticky. Učitel předepsal chybové kódy
(Moodle poznámka „Poznámky k tvorbě LL gramatiky“: „za proměnnou
nesmí být závorka“ znamená chybu 2). Gramatika a LL tabulka se
nemění, mění se jen akce v `parse_rhs` a `parse_stmt_id_tail`
(volba větve podle symtable a lookaheadu, stejná technika jako
`strcmp` u `ifj26` a `main`).

| `symtable_kind(id)` | další token | akce |
|---|---|---|
| funkce | `(` | volání (postfix výrazu, §15) |
| funkce 0 skupin | jiný | VOLÁNÍ bez závorek (specifikace §5.10), i ve výrazu (§14.5) |
| funkce ≥1 skupina | jiný | chyba 2 (funkce není hodnota; eta-expanze až LAMBDAS) |
| proměnná | `(` | chyba 2 (za proměnnou nesmí být závorka) |
| proměnná | jiný | výraz (`ExprFromId`) |
| nedefinováno | `(` | chyba 3 (nedefinované) |
| nedefinováno | jiný | výraz, chyba 3 při lookupu |

Od čistě syntaktického dispatche se to liší u `x = f` a `f + 1`
(f je funkce) a `x = f(1)` (f je proměnná): syntakticky by to byl
výraz nebo volání a umřelo by to až sémanticky, podle symtable
to zemře hned chybou 2.

Pořadí průchodů (povinné): specifikace §2.3 říká „Funkci je možné
volat před její lexikální definicí, podporována je i vzájemná
rekurze“. Dispatch proto vyžaduje předprůchod: nejdřív zaregistrovat
signatury všech funkcí, teprve pak parsovat těla a rozhodovat o druhu
symbolu. Neznámý symbol po předprůchodu je chyba 3.

## 6. Argumenty volání

```ebnf
ArgList       → Expr ArgRest                 (* FUNEXP: argumenty = výrazy *)
              | ε                            (* FIRST(Expr) | {RPAREN} *)
ArgRest       → COMMA AfterCommaArg
              | ε
AfterCommaArg → Expr ArgRest
              | ε                            (* koncová čárka: jen s EOL-flagem *)
```

Poznámka: tato pravidla nejsou v LL tabulce - seznam argumentů
zpracovává výrazová analýza jako součást výrazu (sdílený `parse_arg_list`
pro volání i metody; koncová čárka jen s EOL-flagem na závěrečné `)` -
vnořené závorky seskupení a volání patří dovnitř výrazu, seznam se zastaví
před čárkou a svou uzavírací `)`). §6 je tedy popis jazyka seznamu pro
výrazovou část, ne pravidla LL sestupu - ta by byla od Program odpojená
(prázdné FOLLOW). `Term` mizí - `()` je součástí obecného výrazu.
Původní řešení (pouze termy) platilo do FUNEXP, který ho nahradil.
- `()` je literál Unit (§1.1: „Literál hodnoty typu Unit se zapisuje
  ()“), proto je legální například `print(())`.
- Koncová čárka: tentýž vzor jako u parametrů v §2, včetně kontroly
  EOL-flagu v ε-větvi `AfterCommaArg`.

## 7. FIRST / FOLLOW

| neterminál | FIRST | FOLLOW | ε? |
|---|---|---|---|
| Program | `IMPORT` | – | ne |
| Prolog | `IMPORT` | `AT DEF EOL EOF` | ne |
| FuncDefs | `AT DEF EXTENSION EOL` | `EOF` | ano |
| ExtensionDef | `EXTENSION` | `AT DEF EXTENSION EOL EOF` | ne |
| ExtDefs | `AT DEF EOL` | `DEDENT` | ano |
| FuncDef | `AT DEF` | `AT DEF EOL EOF` | ne |
| OptEol | `EOL` | `DEF` | ano |
| Params | `LPAREN` | `ASSIGN COLON` | ano |
| ParamGroup | `LPAREN` | `AT DEF EXTENSION EOL ASSIGN COLON RPAREN` | ne |
| ParamList | `ID` | `RPAREN` | ano |
| ParamRest | `COMMA` | `RPAREN` | ano |
| AfterCommaParam | `ID` | `RPAREN` | ano |
| Param | `ID` | `COMMA RPAREN` | ne |
| RetType | `COLON` | `ASSIGN` | ano |
| Type | `INT_TOKEN DOUBLE_TOKEN STRING_TOKEN UNIT_TOKEN BOOLEAN_TOKEN` | `ASSIGN COMMA RPAREN` | ne |
| IfThen | `EOL` ∪ FIRST(Rhs) | stmt-starty, `DEDENT EOL` | ne |
| IfTail | `ELSE` | stmt-starty, `DEDENT EOL` | ano |
| IfElseOpt | `ELSE` ∪ `EOL SEMICOLON` | stmt-starty, `DEDENT EOL` | ne |
| IfElse | `EOL` ∪ `IF` ∪ FIRST(Expr)−`IF` | stmt-starty, `DEDENT EOL` | ne |
| ForStmt | `FOR` | stmt-starty, `DEDENT EOL` | ne |
| ForHead | `EOL` ∪ `ID` | stmt-starty, `DEDENT EOL` | ne |
| ForGen | `ID` | `IF EOL DEDENT` | ne |
| ForGuardRest | `EOL IF` | `DEDENT` | ano |
| DoBody | `EOL` ∪ FIRST(Rhs) | stmt-starty, `DEDENT EOL` | ne |
| Range | FIRST(Expr) | `EOL DO` | ne |
| RangeTail | `TO UNTIL` | `EOL DO` | ne |
| RangeBy | `BY` | `EOL DO` | ano |
| InterpString | `INTERP_START` | (satelit) | ne |
| InterpContent | `ID INTERP_MID INTERP_END LBRACE` | (satelit) | ne |
| Body | `EOL ID INT_NUM DOUBLE_NUM STRING_LITERAL LPAREN PLUS MINUS TRUE FALSE NOT` | `AT DEF EOL EOF` | ne |
| Block | `VAL VAR IF WHILE FOR RETURN ID INT_NUM DOUBLE_NUM STRING_LITERAL LPAREN TRUE FALSE INTERP_START` | `DEDENT` | ne |
| LineEnd | `EOL SEMICOLON` | stmt-starty, `DEDENT EOL` | ne |
| MidItem | stmt-starty ∪ unární ∪ `EOL IF WHILE FOR` | stmt-starty, `DEDENT EOL` | ne |
| MidSimple | stmt-starty ∪ unární | `EOL SEMICOLON` | ne |
| BlockItem | (též jako Block) | stmt-starty, `DEDENT EOL` | ne |
| StmtRest | `EOL`, stmt-starty (bez unárních) | `DEDENT` | ano |
| SimpleStmt | `VAL VAR RETURN ID INT_NUM DOUBLE_NUM STRING_LITERAL LPAREN TRUE FALSE` | `EOL SEMICOLON` | ne |
| ValStmt / VarStmt | `VAL` / `VAR` | `EOL SEMICOLON` | ne |
| ValVarTail | `ASSIGN COLON` | `EOL SEMICOLON` | ne |
| IfStmt / WhileStmt | `IF` / `WHILE` | stmt-starty, `DEDENT EOL` | ne |
| ReturnStmt | `RETURN` | `EOL SEMICOLON` | ne |
| RhsOpt | `ID INT_NUM DOUBLE_NUM STRING_LITERAL LPAREN PLUS MINUS TRUE FALSE NOT` | `EOL SEMICOLON` | ano |
| Rhs | `ID INT_NUM DOUBLE_NUM STRING_LITERAL LPAREN PLUS MINUS TRUE FALSE NOT` | `EOL SEMICOLON` | ne |
| Rhs | (pseudoterminál - alias pro Expr, §5) | - | ne |
| StmtIdTail | `ASSIGN`, operátory, `EOL SEMICOLON LPAREN` | `EOL SEMICOLON` | ne |
| ArgRest | `COMMA` | `RPAREN` | ano |

Pseudoterminály (interface, bez pravidel): `FIRST(Expr)` = FIRST(Rhs);
`FIRST(ExprStmt)` = FIRST(Expr) bez `ID` a unárních `+`/`-`/`!` (§13.8);
`FIRST(ExprNoId)` = FIRST(Expr) bez `ID`; `FIRST(ExprFromId)` = binární
operátory a `{EOL, SEMICOLON}` (výraz může být jen to samotné ID).

## 8. LL tabulka

Tabulka je samostatně v `grammar_table.md` (0 kolizí; prázdná buňka = syntaktická chyba 2). Při každé změně pravidel
skript tabulku a testy znovu zkontroluje.

## 9. Verifikace trasováním

`ll_check.py` trasuje tokenové streamy tabulkovým parserem. Výrazy
modeluje automat operandů a operátorů s validací koncového stavu
(visící operátor, nezavřená závorka, nedokončený obsah závorky,
řetězení relačních operátorů).

Legální vstupy (výběr, 65/65 celkem): případy A až G; vnořené bloky;
prázdné řádky v bloku i před DEDENT; `;` s následujícím EOL; koncová
čárka s EOL-flagem (parametry i argumenty); všechny tvary val/var;
přiřazení, přiřazení voláním, volací příkaz, více argumentů; výrazový
příkaz od id i od literálu; unární minus; `` jako příkaz i tělo;
podmínka v závorkách; while; return s výrazem, voláním i holý;
jednořádkové tělo voláním; @main s blokem i na samostatném řádku;
`print()`; prázdné řádky mezi definicemi i po importu; jen import
bez definic; faktoriál; celý program (add + @main).

Nelegální vstupy, všechny končí chybou 2:

| vstup | kde to skončí |
|---|---|
| `val a = 1 val b = 2` | `LineEnd × VAL`, prázdná buňka |
| `def f(a: Int,)` na jednom řádku | `AfterCommaParam`: ε bez EOL-flagu |
| `foo(a,)` na jednom řádku | `AfterCommaArg`: ε bez EOL-flagu |
| `var x: Int` bez inicializace | `ASSIGN × EOL` |
| `val c: Int⏎ = 3` (i s prázdnými řádky) | `ASSIGN × EOL` (§4.1) |
| `val c =⏎    3` (RHS na dalším řádku) | `Rhs × EOL` (§4.1) |
| `val x = 1 +` (visící operátor) | validace koncového stavu |
| `val x = (1 + 2` (nezavřená závorka) | validace koncového stavu |
| `val x = (1 +)` | validace koncového stavu |
| `if` s prázdnou podmínkou | validace koncového stavu |
| `a < b < c` (řetězení relací) | validace koncového stavu |
| `x = a + f(1)` | výrazová analýza se zastaví před `(`, `LineEnd × LPAREN` |
| `x = f(1) + 2` | volání hotové, `LineEnd × PLUS` |
| `return a + f(1)`, `return f(1) + 2` | též |
| `if f(1) then …` | očekáváno THEN, přišel LPAREN |
| `x = y = 1` | `IdTail × ASSIGN`, přiřazení není výraz |
| `if …` bez else | očekáváno ELSE |
| chybějící INDENT po then | očekáváno INDENT |
| příkaz na nejvyšší úrovni | `FuncDefs × VAL`, prázdná buňka |

## 10. Co v gramatice není

- Výrazy samotné (operátory, priority, závorky uvnitř). Gramatika zná
  jen styčné body (§5); obsah řeší precedenční tabulka.
- Komentáře a bílé znaky, ve streamu neexistují.
- Sémantické kontroly: právě jedna `@main` (chybí → 3, více → 4,
  parametr → 5, jiný návratový typ než Unit → 6); levá strana
  přiřazení musí být `var` (7); návratový typ povinný při rekurzi
  nebo výskytu return (6); přetěžování není podporováno (4);
  redefinice (4); použití před definicí (3); typová kompatibilita (6).
- Rozšíření (for, match, true, …). Žádné pravidlo znamená, že klíčové
  slovo rozšíření skončí chybou 2, v základním zadání správně.
  Pro CONTROL: `to`, `until`, `by` zůstanou identifikátory a parser
  u for-hlavičky ověří jejich atribut (doporučení učitele na fóru,
  scanner nemění nic).

## 11. Rozhodnuté otázky a jejich zdůvodnění

1. Holý `return`: připouštíme (`RhsOpt → ε`). Specifikace §2.3:
   „Samostatný return vrací  a je povolen pouze ve funkci
   s explicitním návratovým typem Unit.“ Syntakticky legální;
   navazující sémantická kontrola: jen explicitní Unit, porušení
   znamená typovou chybu 6. Doplňkové pravidlo tamtéž: funkce
   s explicitním Unit vyhodnotí návratovou hodnotu a zahodí ji
   (i u return), sémantika tedy nesmí mechanicky vyžadovat typ Unit
   u vraceného výrazu.
2. Volání v podmínce a v returnu: FUNEXP (§15) dělá volání
   součástí výrazu. `return subStr(text, 0, last)` z `strings.scala`
   (ř. 12 a 37) proto prochází přes `Rhs` stejně jako `if f(1) then`
   přes `Expr`.
3. Dispatch úvodního ID: učitelova varianta se symtable (§5.1,
   Moodle poznámka). Fórum „Čistá LL(1)_gramatika“ (Ondryáš,
   1. 11. 2026) potvrzuje, že odchylky od čisté LL jsou povolené,
   pokud jsou v dokumentaci popsány. Nedefinované ID (s `(` i bez)
   znamená chybu 3.
4. Prázdný řádek mezi hlavičkou a blokem: tolerujeme.
   Specifikace §1.2: „Fyzické řádky, které jsou pouze prázdné,
   obsahují jen bílé znaky či komentáře, jsou ignorovány.“
   a „Prázdné řádky a řádky tvořené jen bílými znaky nemění aktuální
   úroveň odsazení.“ Prázdné řádky jsou pro syntaxi neviditelné.
5. Přesné názvy tokenů z `token.h` se srovnají mechanicky před
   přepisem do C (§12 grammar.md).
6. `;` mezi definicemi: odmítáme. §1.2 dává středník jako oddělovač
   příkazů; §2 říká, že za prologem jsou definice funkcí, a definice
   nejsou příkazy. V žádné ukázce učitelů se středník nevyskytuje.
   `;` za jednořádkovým tělem if/for naopak přijímáme: `LineEnd`
   má větev `SEMICOLON MidItem` (§3). Multi-line konstrukce
   ukončuje DEDENT, středník nemají.
7. Typ a `=` na různých řádcích: chyba 2. Zdůvodnění v §4.1
   (logický řádek, jedna forma val, směr platnosti Scaly, asymetrie
   odsazení).

## 12. Mapování do parser.c

| neterminál | funkce | poznámky ke kódu |
|---|---|---|
| Program | `parse_program` | |
| Prolog | `parse_prolog` | po matchi ID: `strcmp(attr,"ifj26")`, jinak chyba 2 |
| FuncDefs | `parse_func_defs` | smyčka: EOL / DEF / AT / EXTENSION / EOF |
| ExtensionDef / ExtDefs | `parse_extension_def` / `parse_ext_defs` | EXTKW |
| FuncDef | `parse_func_def` | větev AT: `strcmp(attr,"main")` |
| OptEol | inline | `if (cur==EOL) advance;` |
| Params / ParamList / ParamRest / AfterCommaParam | `parse_params` a další | ε-větev AfterCommaParam: check EOL-flagu |
| Param | `parse_param` | |
| RetType | `parse_ret_type` | |
| Type | `parse_type` | switch přes 4 tokeny |
| Body | `parse_body` | |
| Block | `parse_block` | |
| BlockItem | `parse_block_item` | kategorie: jednoduchý → SimpleStmt + LineEnd; složený → If/While/For |
| StmtRest | `parse_stmt_rest` | smyčka: EOL / další / konec |
| LineEnd / MidItem / MidSimple | `parse_line_end` / `parse_mid_item` / `parse_mid_simple` | za ; unární OK, trailing ; přes MidItem→EOL |
| SimpleStmt | `parse_simple_stmt` | switch přes první token |
| ValStmt / VarStmt | `parse_val_stmt` / `parse_var_stmt` | společný `parse_val_var_tail` |
| ValVarTail | `parse_val_var_tail` | jednořádkové (§4.1) |
| IfStmt / WhileStmt | `parse_if_stmt` / `parse_while_stmt` | |
| IfThen / IfElseOpt / IfElse | `parse_if_then` / `parse_if_else` | jednořádek/blok/else-if řetěz |
| ForStmt / ForHead / Range | `parse_for_stmt` / `parse_for_head` / `parse_range` | to/until/by = ID+strcmp |
| DoBody / ForGuardRest | `parse_do_body` / `parse_guards` | |
| ReturnStmt | `parse_return_stmt` | ε-větev `RhsOpt` = holý return |
| RhsOpt | inline | |
| Rhs | `parse_rhs` | volání výrazové analýzy (alias Expr); symtable dispatch uvnitř ní |
| StmtIdTail | `parse_stmt_id_tail` | ASSIGN → parse_rhs; jinak `parse_expression_from(saved)` (LPAREN = volání) |
| ArgList / ArgRest / AfterCommaArg | `parse_args` a další | flag u AfterCommaArg |
| Expr / ExprNoId / ExprFromId | precedenční analýza | `parse_expression`, `parse_expression_from(tok)`; stopping: EOL, SEMICOLON, THEN, DO, **COMMA, RPAREN** (čárka a uzavírací závorka seznamu argumentů - seskupovací a vnořené závorky patří dovnitř výrazu, §6) |

## 13. Rozšíření BOOLTHEN

Specifikace §5.2 (PDF): přidává typ `Boolean`, literály `true` a `false`,
unární negaci `!`, binární operátory `&&` a `||`, vestavěnou funkci
`readBoolean`, zkrácený podmíněný příkaz (bez `else`) a větve `else if`.
Změněná pravidla jsou v §2, §4 a §6; zde zdůvodnění.

### 13.1 Literály `true` a `false`

Jsou to termy jako každý jiný literál: `Term` má dvě nové varianty,
projdou tedy i jako argumenty volání (`negate(true)`). Zároveň jsou
operandem výrazu, takže FIRST(Expr) přibralo `TRUE_TOKEN` a `FALSE_TOKEN`
a tabulka se přepočítala v celých řádcích dotčených pravidel (SimpleStmt,
Rhs, RhsOpt, Body, Block, BlockItem, StmtRest). Nové tokeny nejsou v žádném
existujícím FIRST ani FOLLOW, nová kolize tedy vzniknout nemohla.

`true` a `false` nejsou v seznamu klíčových slov §1.1 - specifikace je
říká literály. Přidání rozšíření jazyk zužuje: to, co bylo (teoreticky)
platným identifikátorem, se rezervuje. Podle §1.1 se na výskyt podtržených
klíčových slov v základním zadání testovat nebude.

### 13.2 Typ `Boolean`

Pátá varianta `Type`. Nový terminál, který není v žádném existujícím FIRST
ani FOLLOW, konflikt vytvořit nemůže: buňka `Type × BOOLEAN_TOKEN` byla
prázdná a nárokuje si ji jediné nové pravidlo.

### 13.3 Volitelné `else` (IfTail)

`IfTail → ELSE IfElse | ε`. Predikt ε-větve je FOLLOW(IfTail) =
stmt-starty ∪ {EOL, DEDENT}; větev s ELSE má Predikt {ELSE}. Množiny jsou
disjunktní - `ELSE` není začátek příkazu ani ukončovač, takže parser
na DEDENT či začátku dalšího příkazu správně pozná konec ifu.

Otestováno v obou polohách: if bez else následovaný dalším příkazem
(StmtRest pokračuje) i jako poslední příkaz bloku (StmtRest končí
na DEDENT).

### 13.4 `else if` a dangling else

Za `else if` stojí celé if znovu: `IfTail → ELSE IfElse` a
`IfElse → IfStmt` (rekurze, ne nová větev). Rozlišení jedním
lookaheadem: `{EOL}` × `{IF}`, čisté. Obvyčejné `else` (blok i
jednořádek) řetěz ukončuje - další `else` skončí na prázdné buňce
(regresní testy v §13.8).

„Zarovnáno s if" (§2.4) je pravidlo scanneru, v LL tabulce neviditelné -
vidíme jen jeho důsledek, počty DEDENTů. Méně odsazený `else if` proto
uvízne stejně jako `else` bez ničeho (prázdná buňka `IfElse × DEDENT`,
případně očekáváno EOL).

Dangling else. S volitelným `else` by `if a then⏎ if b then …⏎ else`
mohlo patřit dvěma ifům. V IFJ26 kolidovat nemůže: else musí ležet
na vlastním logickém řádku odsazeném stejně jako jeho if (§2.4) a scanner
vrací přesně jeden DEDENT na úroveň odsazení (kontrakt b). Tokenový proud
proto nese počet DEDENTů odpovídající tomu, kolik bloků se mezi then
a else zavřelo - gramatika dostane jednoznačnou informaci, ke kterému
if else patří (test „if bez else poslední v bloku, ELSE patří vnějšímu").

### 13.5 Operátory `!`, `&&`, `||`

`!` je unární operátor jako `+` a `-`: FIRST(Expr) přibralo `NOT_TOKEN`
(tělo `def negate(…): Boolean = !value` začíná vykřičníkem, projde
cestou ExprNoId).

`&&` a `||` nemění jediné pravidlo gramatiky. Jsou binární operátory
uvnitř výrazů - věc precedenční tabulky. Gramatiku se dotýkají jedině
nepřímo: výraz může pokračovat od uloženého ID, takže FIRST(ExprFromId)
přibralo `AND_TOKEN` a `OR_TOKEN` (buňky IdTail a StmtIdTail × `&&`/`||`).
Stopping tokeny (EOL, SEMICOLON, THEN, DO) se nemění - `&&` nikdy
nezačíná výraz ani neukončuje příkaz; jinde než mezi operandy jde o chybu 2
(prázdná buňka) nebo neúplný výraz (visící operátor).

### 13.6 Podmínka if a while

„Libovolný výraz typu Boolean" (§2.4) místo „relační výraz" (§2.4) -
syntakticky beze změny. `IfStmt` měl od začátku `IF Expr THEN`, takže
holá proměnná i `!enabled` prochází stejnou cestou; „relační" bylo
sémantické omezení (typová kontrola), ne tvar.

### 13.7 Překlopený test základu a otevřená otázka

Test „if bez else → chyba 2" ze základu je překlopen na legální: jediná
gramatika pro všechna rozšíření nemá jak odlišit program základní od
programu s BOOLTHEN. Otevřená strategická otázka (fórum): musí překladač
s implementovaným BOOLTHEN odmítat `if` bez `else` v programech, které
jinak rozšíření nepoužívají? Obdobně se bude řešit každé další rozšíření
měnící povinné části základu.

### 13.8 Opravy po externím review

Tři chyby nalezené review, ověřené na kontrolním parseru a opravené:

1. Obyčejné `else` ukončuje řetěz větví. Původní
   `IfElse → EOL INDENT Block DEDENT IfTail` přijímal i
   `if … else … else …` a `if … else … else if` - gramatika zvolila
   pravidlo jednoznačně (žádná kolize), jen pravidlo přijímalo příliš
   mnoho. Řetězení je dáno výhradně přes `else if` (§13.4): bloková
   větev else je vždy poslední, rekurzivní `IfStmt` v druhé větvi má
   svůj vlastní `IfTail`, takže řetěz `else if … else if … else`
   funguje dál. Regresní testy: dvojitý `else` a `else`-blok následovaný
   `else if` → chyba 2.
2. Výrazový příkaz nezačíná unárním operátorem. §1.2: logický řádek
   nemůže začínat operátorem. Příkaz začíná logickou řádkou, proto
   `SimpleStmt → ExprStmt` s `FIRST(ExprStmt)` bez `+`, `-` a `!`
   (mezera u `+`/`-` existovala už od základní verze; `!` ji rozšířil
   BOOLTHEN). Pozice uprostřed řádku jsou beze změny: `val x = !true`,
   `if !x then`, `def f: Boolean = !value` zůstávají legální
   (výraz za `=`, `then`, v těle na stejném logickém řádku).
3. Relační řetězy se počítají po `&&`/`||`. Model výrazu odmítal
   legální `1 < 2 && 3 < 4`, protože počítal všechna porovnání na úrovni
   dohromady. Podle §5.2 mají relační operátory vyšší prioritu než `&&`,
   tedy každé porovnání tvoří vlastní podvýraz a spojka `&&`/`||`
   řetěz uzavírá. Počítadlo se resetuje při `&&`/`||`; řetězení uvnitř
   jednoho podvýrazu (`1 < 2 < 3`) i nadále chyba.

## 14. Rozšíření PARAMLISTS + VALMEM + EXTKW

Tři rozšíření najednou; společný jmenovatel je postfixový model
výrazu. Změněná pravidla v §1, §2 a §5; zde zdůvodnění.

### 14.1 PARAMLISTS: 0 až n skupin parametrů

`Params → ParamGroup Params | ε` - původní pravidlo se stalo `ParamGroup`;
`Params` je nullable celek. Predikt: `{LPAREN}` × FOLLOW `{COLON, ASSIGN}`
- `(` není typ ani `=`, čisté. `def answer = 42` je tak syntakticky úplně
běžný FuncDef; volá se bez závorek a každé použití tělo vyhodnotí znovu
(§5.10: „nejde o proměnnou“). Rozlišení proměnná × 0-skupinová funkce
u `val x = answer` jde do symtable dispatche (§5.1) - další případ
sémantického dispatche jako `x = f(1)`.

Volání se všemi skupinami: `IdTail → LPAREN ArgList RPAREN ArgGroups`.
`add(1)(2) + 3` padne na prázdné buňce `ArgGroups × PLUS` - volání ve
výrazu je FUNEXP. Prázdný seznam argumentů nelze vynechat
(§5.10); chybný počet seznamů je chyba 5.

### 14.2 VALMEM: `.člen` jako postfix

Členy `toString`, `toInt`, `floor`, `length` (§5.4 - právě tyto čtyři;
`.floor` nemá vestavěný protějšek, na rozdíl od zbývajících tří, které
aliasují `toStr`/`toInt`/`strLength`). Gramatika se mění jediným dotykem:
`FIRST(ExprFromId) += DOT` - tedy buňky `IdTail × DOT` a `StmtIdTail × DOT`
vedou do `ExprFromId`. Samotný postfix patří výrazové analýze (§14.4).
Visící tečka (`x.` EOL) = neúplný výraz; tečka nezačíná výraz
(`Rhs × DOT` prázdná buňka = chyba 2).

### 14.3 EXTKW: extension bloky

`FuncDefs` přibrala větev `ExtensionDef` - nový terminál `EXTENSION`
není v žádném existujícím FIRST/FOLLOW, jako dříve `BOOLEAN_TOKEN` u BOOLTHEN.
Tvar bloku odpovídá ukázce §5.9: hlavička s jedním explicitně typovaným
parametrem-příjemcem, EOL, odsazené metody (`ExtDefs`, tentýž vzor
seznamu jako `FuncDefs`). Bezparametrické metody bez závorek
(`def twice: Int`) a metody s prázdným seznamem (`def foo: String`)
jsou 0- a 1-skupinové funkce z §14.1. `@main` uvnitř extension je věc
sémantiky. Argumenty metody za `.člen` (`x.plus(2)`, `x.plus(1)(2)`)
zpracovává výrazová analýza - sdílený `parse_arg_list` s IdTail-větví
(koncová čárka + EOL-flag jedna implementace).

### 14.4 Architektura: postfixový řetěz patří výrazové analýze

S přibývajícími VALMEM + EXTKW (+ později LISTS, ALGEBDT) výrazová
strana konverguje k modelu *primární výraz + postfixový řetězec*
`výraz (.člen | (args))*`. Řetězec je věc výrazové analýzy, ne
gramatiky:

1. Postfix visí na libovolných operandech - `x.toString`,
   `"abc".length`, `(a + b).toString`, `1.plus(2)` - ne jen na úvodním
   id. Gramatická pravidla by musela trhat výraz v půlce.
2. LL tabulka zůstává triviálně čistá - postfix nikdy nezačíná výraz
   ani neukončuje příkaz; jediný dotek jsou dvě buňky přes `DOT`.
3. Dávka 10 (LAMBDAS) tam stejně soustředí všechno těžké - rozlišení
   `(x: Int) => …` od `(x + 1)` vyžaduje lookahead na odpovídající `)`,
   což LL tabulka neumí, ale výrazová analýza ano (najdi párovou `)`,
   zkus `=>`). Konvergence k ní tedy není hromadění rizika, ale přesun
   těžkostí tam, kde se nevyhnou.
4. Dělba práce zůstává čistá: gramatika vlastní strukturu programu,
   výrazová analýza výrazy; `Rhs`/`StmtIdTail` jsou tenká dispatch vrstva.

Dávka 6 (FUNEXP) je pak subtraktivní delta: `IdTail` a `ArgGroups`
zmizí (volání = postfix na libovolném operandu), argumenty se stanou
výrazy a tabulka se smrskne.

### 14.5 Rozšíření dispatch tabulky (symtable)

Symtable u funkce zná počet skupin (0, 1, 2+). Odpovědi z PDF:
`answer` = chybný počet seznamů → chyba 5 (§5.10); `val x = f`
(f má ``) → chyba 2 (funkce není hodnota; přiřazení funkce
 až s LAMBDAS, a to jen s explicitním typem); 0-skupinová funkce
v podmínce → legální (§5.10: „součástí výrazu i bez FUNEXP“);
`@main` s nula skupinami → legální (0 skupin = bez parametrů).
`.name` na hodnotě: vestavěný člen → extension metoda → chyba
(3 nebo 5, dle druhu). `x.plus(2) + 1`: členské volání JE výraz
(VALMEM „tento zápis tvoří výraz“) - k legalitě bez FUNEXP se
v testech dávky 5 přikláníme k legálnímu, sondu má potvrdit.

### 14.6 Opravy po externím review

Čtyři nálezy review, ověřené na kontrolním parseru a opravené:

1. Unární operátory za středníkem. §1.2 zakazuje operátor na začátku
   logického řádku; `print(1); !true` je na stejném řádku a tedy legální.
   Řešení §3: `LineEnd/MidItem/MidSimple` - cesta za `SEMICOLON` má vlastní
   statement (`MidSimple` s unárními), začátek řádku (`SimpleStmt/ExprStmt`)
   je bez nich. Trailing `;` řeší `MidItem → EOL` (ne ε - viz §3).
   Regresní testy: `print(1); !true` OK, `val x = 1; -x` OK, `!true` na
   vlastním řádku stále chyba 2.
2. Literál `` v argumentech metody. `_args` (model výrazové analýzy
   v ll_check) znal jen jednotokenové termy; `1.take()` odmítal. Nyní
   `` jako term všude (shoda s `Term` v §6). Chyba modelu, ne gramatiky -
   plán sdílet `parse_arg_list` mezi IdTail-větví a metodami zůstává.
3. Argumenty přes uzavřenou závorku. `("x".length)` procházel, protože
   `after_member` přežil zavření závorky. Po uzavření závorky se resetuje -
   `` za uzavřeným výrazem je volání na výrazu = FUNEXP, chyba 2.
   Řetěz skupin u metody (`1.m(2)(3)`) funguje dál.
4. Soulad dokumentace. §5.1 dispatch: funkce s 0 skupinami se bez závorek
   VOLAJÍ (§5.10, §14.5) - řádek „funkce + jiný → chyba 2" platí jen pro
   ≥1 skupinu. FIRST `Block`/`StmtRest` v §7 odpovídá vypočtené tabulce
   (bez unárních - `ExprStmt` je na začátku řádku zakazuje).

## 15. Rozšíření FUNEXP

Specifikace §5.1: „Volání vestavěné i uživatelské funkce může být součástí
výrazu. Argumentem funkce může být libovolný výraz. Volání lze vnořovat
a argumenty se vyhodnocují zleva doprava.“ Naplňuje slib z §14.4 -
subtraktivní delta: `IdTail`, `ArgGroups` a `Term` mizí, `Rhs` se stává
aliasem pro `Expr`, argumenty jsou výrazy.

### 15.1 Co se změnilo

| oblast |  |  |
|---|---|---|
| volání | `IdTail → LPAREN ArgList RPAREN ArgGroups` | postfix výrazu - věc výrazové analýzy |
| `Rhs` | `ID IdTail \| ExprNoId` | pseudoterminál - alias pro `Expr` |
| `StmtIdTail` | `ASSIGN Rhs \| IdTail` | `ASSIGN Rhs \| ExprFromId` (FIRST += `LPAREN`) |
| argumenty | termy (`Term`) | výrazy (`Expr`; sdílený `parse_arg_list` s metodami) |
| `f(1) + 2`, `f(1).toString`, `add(1)(2) + 3` | chyba 2 | OK (volání = postfix) |
| `(1+2)(3)`, `1(2)` | chyba 2 | tvar OK (model); platnost rozhodne deklarace |

Rozlišujeme přijetí tvaru modelem a platnost volání podle deklarace:
`(1+2)(3)` ani `1(2)` nemohou být nikdy platná (volat lze jen funkci;
Int není funkce - sémantická odmítnutí), ale model tvar správně rozpozná
a kontrola volání ho odmítne. `add(1)(2)` u funkce se dvěma skupinami
je jedno volání se dvěma seznamy argumentů (specifikace §5.10: tělo se provede
až po vyhodnocení obou skupin) - ne dvě volání.

Přiřazení není výraz: `x = y = 1` zůstává chyba 2 - proto `StmtIdTail`
přežívá jako jediná dispatch vrstva na úrovni příkazu. `x(1) = 2` umírá
na `LineEnd × ASSIGN`. Podmínka if/while je výraz → `if f(1) then`
je syntakticky legální (sémantika: návratový typ Boolean).

### 15.2 after_operand (kontrakt s výrazovou analýzou)

Příznak `after_member`  se mění na `after_operand` - LPAREN
v operátor-stavu po libovolném operandu = volání. Přechody:

| událost | after_operand |
|---|---|
| start `ExprFromId` (uložené ID je operand) | True |
| operand (id, literál, TRUE/FALSE) | True |
| unární `+ - !` (čeká se operand) | nemění (False) |
| `` - prázdná závorka | True |
| `)` - zavření neprázdné závorky | True (.1 reset;  přetočeno - zavřený výraz je operand) |
| binární operátor | False |
| `.člen` | True |
| argumentová skupina `_args` | True (zůstává - řetězy `f(1)(2)`) |

`1(2)` - volání na literálu - je syntakticky legální; sémantika odmítne
(kód k dořešení: 5 × 6, sémantická past). `f` na 0-skupinové funkci →
chyba 5 (špatný počet seznamů, §5.10). Řetězy `f(1)(2)(3)`
a mixy `f(1).g(2).h(3)` fungují přes after_operand bez omezení délky.

### 15.3 Sémantika dispatch (§5.1, doplněk)

FUNEXP nemění symtable pravidla - volání se jen může objevit na více
místech. `val x = f` (f ≥1 skupina) = nadále chyba 2 (funkce není hodnota,
eta-expanze až LAMBDAS); `f` 0-skupinová = volání bez závorek (§14.5).

### 15.4 Výhled (LAMBDAS)

Výrazy zpracovává celé výrazová analýza - operátory, priority,
závorky, volání, postfix. To je předpoklad LAMBDAS:
rozlišení `(x: Int) => …` od `(x + 1)` vyžaduje najít párovou `)`
a zkusit `=>` - LL tabulka to neumí, recursive descent výrazové
analýzy ano. CONTROL (if-výraz) a LISTS (indexování `lst(2)`)
z téhož konvergence těží.

## 16. Rozšíření CONTROL

Specifikace §5.3: if lze použít jako výraz (else povinný); jednořádková
těla všech větví; for s jedním generátorem (`to`/`until`/`by`) a strážemi.

### 16.1 Jednořádková těla if (příkazová rovina)

EOL-asymetrie (§4): bloková větev končí DEDENT (nic za ní - `IfTail → ε`),
jednořádková EOL - terminátor patří větvi jako u `Body → Rhs EOL`.
Proto dvě téměř shodná pravidla (`IfTail` ε × `IfElseOpt` EOL) - LL
je amnestická, tentýž vzor jako `LineEnd`/`MidSimple`. Důsledek:
`if c then a; b` = chyba 2 (jednořáková větev je jeden výraz - konzistentní
s těly funkcí). Visící `else` (`if c then x⏎else y`) = chyba 2 -
if bez else je po EOL dokončen (konzistentní se Scalou).

### 16.2 if jako výraz (výrazová rovina)

`IF` vstupuje do FIRST(Expr) - if-výraz je operand výrazové analýzy
(srov. volání ve FUNEXP, `.člen` ve VALMEM - třetí potvrzení §14.4).
Skládá se všude: `(if c then a else b) + 1`, `f(if c then a else b)`,
`return if …`. Ve výrazové rovině je else povinný; `else-if` rekurzí.
`+ 1` patří do else-větve (`if c then a else b + 1` - else-větví je
`b + 1`). Struktura: `IF cond⟨stop na THEN⟩ THEN větev ELSE větev` -
větev = jednořádek (greedy) nebo break-form (`EOL INDENT výraz EOL
DEDENT`, spec ukázky abs2/abs3 - obojí potvrzeno: break může být za THEN
i za ELSE).

### 16.3 BROKE - odchylka od LL (opraveno po review)

Break-form spolkne EOL své větve - a to je EOL, které by ukončovalo
nadřazený příkaz. `val abs2 = if … then⏎ 0 - x⏎ else⏎ x` proto končí
DEDENTem jako víceřádkový příkaz (jako if/while) - bez LineEnd. LL
to předem rozhodnout nedokáže: **řešením je test na poslední token
výrazu = DEDENT** (; původní booleovský příznak BROKE byl po review
nahrazen - rozhoduje jak skončil CELÝ výraz, ne zda někde uvnitř byl
break). Terminátor se vynechá jen když aktuální token není EOL
(prázdný řádek se konzumuje normálně). Tabulka se nemění - odchylka
třídy symtable dispatch (§5.1; fórum: odchylky povoleny, pokud popsané).

Blokové větve = plný Block : `break-form = EOL INDENT Block
DEDENT` - blok obsahuje i příkazy (val, if, …), hodnotu větve určuje
poslední příkaz (§2.3). Blok zpracovává syntaktická analýza
(kontrakt: výrazová volá statement parser pro blokové větve if-výrazu).

### 16.4 ForStmt

Dva tvary hlavičky: jednořádkový (`for i <- 1 to 10 by 2 do`) a
víceřádkový (generátor a stráže na vlastních logických řádcích
odsazené pod `for`, `do` na úrovni `for` - control.scala). Lookahead
za FOR: `{EOL}` × `{ID}`. Stráže 0..n, jen ve víceřádkové hlavičce
(inline stráž → prázdná buňka; spec stráže popisuje jen u víceřádkového
zápisu). Tělo (`DoBody`) zrcadlí `Body`: blok nebo jednořádek.
Jeden generátor, žádné yield, for není výraz (vše chyba 2 - spec).

### 16.5 Range: to/until/by = ID + kontrola atributu

`Range → Expr RangeTail`, `RangeTail → TO Expr RangeBy | UNTIL Expr`
(v kódu ID + `strcmp`, technika `ifj26`/`main` - doporučení učitele
z fora). By jen u `to` - §5.3 výslovně uvádí krok pouze u inkluzivního
tvaru `from to to by step`; `until` bez `by` (po review). Kompozice s výrazy je zdarma: `to`/`until`/`by` jsou
neoperátory - výraz se na nich zastaví bez jediného nového stopping
pravidla. PDF: „Všechny meze i krok jsou výrazy typu Int" - plné
výrazy včetně `by -2` a `by f` (FUNEXP). `by 0`: literál → překladová
kontrola → chyba 10 (§6); výraz → generovat runtime kontrolu → 40.
Řídicí proměnná = nová nemodifikovatelná Int platná v těle (zápis → 7).

### 16.6 Kontrakt s výrazovou analýzou (doplněk)

| oblast |  |  |
|---|---|---|
| primární výrazy | id, literály, ``, volání | **+ IF** (if-výraz, rekurze) |
| stopping tokeny | EOL, SEMI, THEN, DO, COMMA, RPAREN | + ELSE (automaticky - neoperátor) |
| layout tokeny | nikdy uvnitř výrazu | jediná výjimka: break-form (jen hloubka 0) |
| after_operand | po operandu/volání/.členu | + po if-výrazu |

## 17. Rozšíření INTERPOLATEDS

Specifikace §5.6: interpolátor `s` jen. `$id` pro hodnotu proměnné,
`${výraz}` pro libovolný výraz. `$$` vloží dolar. Víceřádková varianta
`s"""…"""` je podporovaná (dvojitá uvozovka v ní je doslova).

```ebnf
InterpString  → INTERP_START InterpContent
InterpContent → LBRACE Expr RBRACE InterpContent
              | ID InterpContent
              | INTERP_MID InterpContent
              | INTERP_END
```

Satelit výrazové analýzy. Z Program se k `InterpString` nedostanu
derivací, tabulka má prázdné FOLLOW. Na rozdíl od `ArgList` 
tu není žádná ε-větev, takže SELECT = FIRST a řádky jsou úplné.
Volá ho greedy v operand-stavu přes `_run` — týž kontrakt jako
`Block` u blokových větví if-výrazu (§16.3).

`$var` interpoluje přesně jeden identifikátor (Scala sémantika).
`s"$x.y"` interpoluje `x`, `.y` je text. `s"$x + 1"` interpoluje
`x`, ` + 1` je text. Operátory a `.člen` fungují jen uvnitř `${…}`.
ID v `$var` podléhá dispatchi §5.1: 0-skupinová funkce se volá,
≥1 skupina → chyba 2, nedefinovaná → 3.

### 17.1 Scanner (tokeny a atributy)

| token | kdy | atribut |
|---|---|---|
| `INTERP_START` | text od `s"` po `$id`, `${` nebo `"` (`$$` je součást textu, ne hranice) | escap `\\` `\"` `\n` `\r` `\t`, `$$` → `$` |
| `INTERP_MID` | neprázdný text mezi interpolacemi | totéž |
| `INTERP_END` | poslední text před `"` nebo `"""` | totéž |
| `LBRACE` | `${` | žádný |
| `RBRACE` | párová `}` | žádný |

Víceřádková varianta `s"""…"""`: LF v textu je povolen (je v atributu),
`"` je doslova, uzavírá `"""`. U jednořádkové `s"…"` je LF chyba 1.

### 17.2 Výrazová rovina

`INTERP_START` vstupuje do FIRST(Expr/Rhs/ExprNoId/ExprNoIf/ExprStmt) —
interpolovaný řetězec je operand jako volání, `.člen` a if-výraz.
Konkrétně:

- `val n = s"$x".length` — `.člen` funguje (after_operand po interp)
- `s"a" + s"b"` — konkatenace
- `f(s"$x" + s"$y")` — jako argument (i výrazový)
- `s"a${if c then 1 else 2}b"` — if-výraz v `${…}`
- `s"a${s"b$x"}c"` — vnořování (rekurze greedy)

Stopping: `RBRACE` automaticky (neoperátor v operátor-stavu, jako
dříve COMMA u argumentů). LF v `${…}` nenastane u jednořádkové
(chyba 1 ze scanneru), u multiline je v atributu.

### 17.3 Sémantika

Každá vložená hodnota se převádí stejným způsobem jako `toStr`
(specifikace výslovně). `s"$d"` pro `d = 1.5` dává `"1.50"`.
Codegen: CONCAT sekvence (konstanta, toStr(x), konstanta, ...).

### 17.4 Co odmítáme

- `f"…"` a jiné interpolátory → ID + STRING_LITERAL → dva operandy
  vedle sebe → chyba 2 (přirozeně, nic zvláštního)
- `$` před číslicí / klíčovým slovem / samotné `$_` → lex chyba 1
- prázdné `${}` → prázdná buňka LBRACE × RBRACE → chyba 2
- interp jako typ → prázdná buňka Type × INTERP_START → chyba 2
- holé `{` `}` v kódu → prázdné buňky → chyba 2
- LF v jednořádkové `s"…"` (i uvnitř `${…}`) → lex chyba 1

