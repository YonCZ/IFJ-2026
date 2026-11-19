# IFJ26 LL Gramatika

Základní zadání s devíti rozšířeními: BOOLTHEN, PARAMLISTS, VALMEM, EXTKW,
FUNEXP, CONTROL, INTERPOLATEDS, LISTS. Všechna jsou zapracovaná, ověřená
a zdokumentovaná v sekcích 13 až 19.

| rozšíření | co přidává | kde |
|---|---|---|
| BOOLTHEN | Boolean typ, literály, `!`, `&&`, `\|\|`, volitelné `else`, `else if` | §13 |
| PARAMLISTS | 0..n skupin parametrů, `def f = 42` | §14 |
| VALMEM | `.člen` postfix na hodnotách | §14 |
| EXTKW | `extension` bloky s metodami | §14 |
| FUNEXP | volání jako součást výrazu, výrazy jako argumenty | §15 |
| CONTROL | jednořádková těla if, if-výraz, for s generátory a strážemi | §16 |
| INTERPOLATEDS | interpolované řetězce `s"..."` a `s"""..."""` | §17 |
| LISTS | seznamy `List[T]`, `++`, rozsahy jako výrazy, for přes seznam | §18 |
| ALGEBDT | sjednocení a součinové typy, `match`, `case` a vzory | §19 |

`ll_check.py` kontroluje kolize LL tabulky a 303 toků tokenů (§9).
Skript ověřuje tvar výrazů, jejich seskupení určuje precedenční tabulka.

## 0. Konvence

- Terminály = tokeny z `token.h`:
  `IMPORT DEF VAL VAR IF THEN ELSE WHILE DO RETURN ID INT_NUM DOUBLE_NUM STRING_LITERAL
  LPAREN RPAREN COMMA COLON SEMICOLON ASSIGN DOT MUL AT EOL INDENT DEDENT EOF
  INT_TOKEN DOUBLE_TOKEN STRING_TOKEN UNIT_TOKEN PLUS MINUS
  TRUE FALSE NOT AND OR BOOLEAN`
  (a operátory výrazů `MUL DIV LT LE GT GE EQ NEQ TO UNTIL BY PLUSPLUS`,
  celé v kompetenci precedenční analýzy). LISTS: `LIST_TOKEN` scanner
  vydává přímo (klíčové slovo `List`, stejně jako `type` či `extension`),
  `LBRACK`/`RBRACK` jen v typovém argumentu `List[T]` (jinde v IFJ26
  nevznikají). ALGEBDT: `TYPE`, `CASE`, `CLASS`, `MATCH`
  jsou klíčová slova, `FATARROW` = `=>`, `UNDERSCORE` = `_`.
  Pozor: `INT_TOKEN` = typ, `INT_NUM` = literál, dva různé tokeny.
  Před přepisem do C mechanicky srovnat přesné identifikátory z `token.h`
  (např. `INT_NUM_TOKEN`, `STRING_LITERAL_TOKEN`).
- Neterminály CamelCase; v C funkce `parse_xxx` ve snake_case.
  Ocasy seznamů mají suffix `Rest` (`ParamRest`, `StmtRest`), celé seznamy
  plurál nebo `List` (`FuncDefs`, `ParamList`).
- `ε` = pravá strana prázdná. Komentáře `(* … *)`.
- `Expr`, `ExprNoId`, `ExprFromId` = interface pseudoterminály pro
  precedenční analýzu (§5): mají FIRST, ale nemají vlastní pravidla;
  v kódu to jsou volání její analýzy. `to`/`until`/`by` (LISTS, §18)
  jsou operátory: ID + kontrola atributu.

## 1. Nejvyšší patro

```ebnf
Program  → Prolog FuncDefs EOF
Prolog   → IMPORT ID DOT MUL EOL          (* v kódu: attr(ID)=="ifj26", jinak chyba 2 *)
FuncDefs → EOL FuncDefs                   (* prázdné řádky po importu / mezi definicemi *)
         | FuncDef FuncDefs
         | ExtensionDef FuncDefs           (*  EXTKW *)
         | TypeDef FuncDefs                (*  ALGEBDT *)
         | CaseClassDef FuncDefs           (*  ALGEBDT *)
         | ε                              (* Predikt: {EOL,DEF,AT,EXTENSION,TYPE,CASE} | {EOF} *)
FuncDef  → DEF ID Params RetType ASSIGN Body
         | AT ID OptEol DEF ID Params RetType ASSIGN Body
                                        (* v kódu: attr(ID za AT)=="main" *)
OptEol   → EOL
         | ε                             (* Predikt: {EOL} | FOLLOW = {DEF} *)

ExtensionDef → EXTENSION LPAREN Param RPAREN EOL INDENT ExtDefs DEDENT
TypeDef      → TYPE ID ASSIGN Type EOL       (*  ALGEBDT: alias, jen vrchol *)
CaseClassDef → CASE CLASS ID LPAREN ParamList RPAREN EOL (*  ALGEBDT: součinový typ *)
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
Type            → TypeAtom TypeUnionRest
TypeAtom        → INT_TOKEN | DOUBLE_TOKEN | STRING_TOKEN | UNIT_TOKEN
                | BOOLEAN_TOKEN                 (*  BOOLTHEN *)
                | LIST_TOKEN LBRACK Type RBRACK (*  LISTS: List[T], vnořitelný *)
                | ID                            (*  ALGEBDT: alias i case class
                                               v typové pozici, symtable lookup *)
TypeUnionRest   → PIPE TypeAtom TypeUnionRest  (*  ALGEBDT: T1 | T2 *)
                | ε
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
je `print(1); !true` legální. Trailing `;`
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
ForGen     → ID LARROW Expr EOL                 (*  LISTS: zdroj = výraz *)
ForGenInline → ID LARROW Expr
ForGuardRest → EOL ForGuardRest               (* prázdné řádky *)
             | IF Expr EOL ForGuardRest       (* stráž *)
             | ε
DoBody     → EOL INDENT Block DEDENT          (* tělo = blok *)
           | Rhs LineEnd                     (* tělo = jednořádek; ; ukončí for *)
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
  `else` je volitelný (BOOLTHEN, §13), `else if` je rekurzivní
  `IfStmt` (§13). Za `DEDENT` větve then následuje `ELSE` přímo.
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
Jejich FIRST a stopping tokeny definuje tabulka v §16.

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
| funkce 0 skupin | jiný | VOLÁNÍ bez závorek (specifikace §5.10), i ve výrazu (§14) |
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
| FuncDefs | `AT TYPE CASE DEF EXTENSION EOL` | `EOF` | ano |
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
| Type | FIRST(TypeAtom) | `ASSIGN COMMA RPAREN RBRACK EOL` | ne |
| TypeAtom | `INT_TOKEN DOUBLE_TOKEN STRING_TOKEN UNIT_TOKEN BOOLEAN_TOKEN LIST_TOKEN ID` | `ASSIGN COMMA RPAREN RBRACK PIPE EOL` | ne |
| TypeUnionRest | `PIPE` | `ASSIGN COMMA RPAREN RBRACK EOL` | **ano** |
| TypeDef | `TYPE` | `AT DEF EXTENSION TYPE CASE EOL EOF` | ne |
| Pattern | `INT_NUM DOUBLE_NUM STRING_LITERAL TRUE FALSE UNDERSCORE ID LPAREN MINUS` | – (satelit, §19) | ne |
| PatTail | `COLON LPAREN` | `IF FATARROW EOL DEDENT` | ne |
| PatNeg | `INT_NUM DOUBLE_NUM` | `COMMA RPAREN IF FATARROW` | ne |
| PatArg | `ID INT_NUM DOUBLE_NUM STRING_LITERAL TRUE FALSE UNDERSCORE MINUS` | `COMMA RPAREN` | ne |
| PatList | FIRST(PatArg) | `RPAREN` | **ano** |
| PatRest | `COMMA` | `RPAREN` | **ano** |
| CaseClassDef | `CASE` | `AT DEF EXTENSION TYPE CASE EOL EOF` | ne |
| IfThen | `EOL` ∪ FIRST(Rhs) | stmt-starty, `DEDENT EOL` | ne |
| IfTail | `ELSE` | stmt-starty, `DEDENT EOL` | ano |
| IfElseOpt | `ELSE` ∪ `EOL SEMICOLON` | stmt-starty, `DEDENT EOL` | ne |
| IfElse | `EOL` ∪ `IF` ∪ FIRST(Expr)−`IF` | stmt-starty, `DEDENT EOL` | ne |
| ForStmt | `FOR` | stmt-starty, `DEDENT EOL` | ne |
| ForHead | `EOL` ∪ `ID` | stmt-starty, `DEDENT EOL` | ne |
| ForGen | `ID` | `IF EOL DEDENT` | ne |
| ForGuardRest | `EOL IF` | `DEDENT` | ano |
| DoBody | `EOL` ∪ FIRST(Rhs) | stmt-starty, `DEDENT EOL` | ne |
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
`FIRST(ExprStmt)` = FIRST(Expr) bez `ID` a unárních `+`/`-`/`!` (§13);
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

Legální vstupy (výběr, 71/71 celkem): případy A až G; vnořené bloky;
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
| `x = y = 1` | `StmtIdTail × ASSIGN`, přiřazení není výraz |
| chybějící INDENT po then | očekáváno INDENT |
| příkaz na nejvyšší úrovni | `FuncDefs × VAL`, prázdná buňka |

Pro review pravidel a psaní testů byla použita AI (zadání §4).

Prompt: „Právě jsem napsal pravidla a tabulku pro LL gramatiku, můžeš je
reviewnout, kde by potenciálně mohla nastat chyba a napsat testy, které
to deterministicky kontrolují (napiš soubor ll_check.py)."

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
| Type / TypeAtom / TypeUnionRest | `parse_type` | atom + smyčka PIPE (sjednocení); ID = symtable lookup |
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
| ForStmt / ForHead / ForGen | `parse_for_stmt` / `parse_for_head` / `parse_for_gen` | zdroj = výrazová analýza (ForSrc) + validace tvaru rozsahu |
| TypeDef / CaseClassDef | `parse_type_def` / `parse_case_class` | jen vrchol |
| Pattern / PatArg | `parse_pattern` / `parse_pat_arg` | satelit volaný z match handleru |
| DoBody / ForGuardRest | `parse_do_body` / `parse_guards` | |
| ReturnStmt | `parse_return_stmt` | ε-větev `RhsOpt` = holý return |
| RhsOpt | inline | |
| Rhs | `parse_rhs` | volání výrazové analýzy (alias Expr); symtable dispatch uvnitř ní |
| StmtIdTail | `parse_stmt_id_tail` | ASSIGN → parse_rhs; jinak `parse_expression_from(saved)` (LPAREN = volání) |
| ArgList / ArgRest / AfterCommaArg | `parse_args` a další | flag u AfterCommaArg |
| Expr / ExprNoId / ExprFromId | precedenční analýza | `parse_expression`, `parse_expression_from(tok)`; stopping: EOL, SEMICOLON, THEN, DO, **COMMA, RPAREN** (čárka a uzavírací závorka seznamu argumentů - seskupovací a vnořené závorky patří dovnitř výrazu, §6) |

## 13. Rozšíření BOOLTHEN

§5.2: `Boolean`, `true`/`false`, `!`, `&&`/`||`, if bez `else`, `else if`.

- Literály jsou termy i operandy. Nové tokeny nejsou nikde jinde
  v FIRST/FOLLOW, kolize nemohla vzniknout. `Boolean` je pátá
  varianta `Type` (prázdná buňka → jedno pravidlo).
- `IfTail → ELSE IfElse | ε`. Predikty disjunktní, `else if` je
  rekurze přes `IfElse → IfStmt`. Obvyčejné `else` řetěz vždy
  ukončuje.
- Dangling else nehrozí: else leží na vlastním řádku zarovnaném
  se svým if (§2.4), scanner vrací přesně jeden DEDENT na úroveň.
- `!` je unární (FIRST(Expr) += NOT). `&&`/`||` jsou binární
  operátory precedenční tabulky, dotek přes FIRST(ExprFromId)
  += AND/OR.
- Podmínka if/while je „libovolný výraz typu Boolean" (§2.4),
  syntakticky beze změny.
- Test „if bez else → chyba 2" překlopen na legální: jediná
  gramatika pro všechna rozšíření nemá jak odlišit program
  základní od programu s BOOLTHEN.

Opravy po kontrole:

1. `else` ukončuje řetěz, dvojitý `else` → chyba 2.
2. Výrazový příkaz nezačíná unárním operátorem (§1.2).
   Uprostřed řádku beze změny.
3. Relační řetězy se resetují po `&&`/`||` (§5.2 priorita).
   `1 < 2 < 3` zůstává chyba.

## 14. Rozšíření PARAMLISTS + VALMEM + EXTKW

Tři rozšíření, společný jmenovatel je postfixový model výrazu.

- PARAMLISTS: `Params → ParamGroup Params | ε`, predikt `{LPAREN}`
  × FOLLOW `{COLON, ASSIGN}`. `def answer = 42` je běžný FuncDef,
  každé použití tělo vyhodnotí znovu (§5.10). Volání se všemi
  skupinami zpracovává výrazová analýza (FUNEXP). Prázdný seznam
  nelze vynechat, chybný počet seznamů → chyba 5.
- VALMEM: členy `toString`, `toInt`, `floor`, `length` (§5.4,
  právě tyto čtyři). Jediný dotek: `FIRST(ExprFromId) += DOT`.
  Visící tečka = neúplný výraz.
- EXTKW: `FuncDefs` přibrala větev `ExtensionDef` (nový terminál
  nikde jinde). Tvar podle ukázky §5.9: příjemce, EOL, odsazené
  metody. Metody bez závorek jsou 0-skupinové funkce.
- Architektura: výraz konverguje k *primární výraz + postfix*
  `výraz (.člen | (args))*`. Postfix visí na libovolných operandech,
  pravidla by trhala výraz v půlce. LL tabulka čistá (dotek jsou
  dvě buňky přes DOT). LAMBDAS stejně potřebuje výrazovou analýzu.
- Dispatch: `answer` bez závorek = chyba 5; `val x = f` (≥1
  skupina) = chyba 2; 0-skupinová v podmínce = legální;
  `.name` na hodnotě: vestavěný → metoda → chyba 3/5.

Opravy po kontrole:

1. Unární operátory za `;` legální (§1.2 zakazuje jen začátek
   řádku). Řeší `MidSimple` v §3.
2. Literál `` jako term v argumentech metody ( chyba modelu).
3. `after_member` reset po uzavření závorky ( volání na výrazu
   = FUNEXP, chyba 2).
4. Soulad §5.1 a §7 s tabulkou.

## 15. Rozšíření FUNEXP

§5.1: volání součástí výrazu, argumenty libovolné výrazy. Subtraktivní
delta: `IdTail`, `ArgGroups` a `Term` mizí, `Rhs` je alias pro `Expr`.

- Rozlišujeme tvar a platnost: `(1+2)(3)` ani `1(2)` nikdy platná
  být nemohou, ale model tvar rozpozná a odmítne sémantika.
- `add(1)(2)` u dvouskupinové funkce je jedno volání se dvěma
  seznamy (§5.10: „teprve poté se provede tělo funkce.").
- Přiřazení není výraz: `x = y = 1` zůstává chyba 2, proto
  `StmtIdTail` přežívá jako jediná dispatch vrstva příkazu.
- `if f(1) then` je syntakticky legální ( typ Boolean řeší
  sémantika).
- after_operand: LPAREN v operátor-stavu po libovolném operandu
  = volání. Řetězy `f(1)(2)(3)` i `f(1).g(2)` bez omezení.
- Sémantika dispatch beze změny (§14).
- Výrazová analýza vlastní celé výrazy: operátory, priority,
  závorky, volání, postfix. Předpoklad LAMBDAS, CONTROL, LISTS.

## 16. Rozšíření CONTROL

§5.3: if jako výraz (else povinný), jednořádková těla, for s jedním
generátorem a strážemi.

- Jednořádková těla: EOL-asymetrie jako u `Body`. Bloková větev
  končí DEDENT (`IfTail → ε`), jednořádková má terminátor patřící
  větvi (`IfElseOpt → LineEnd`). `if c then a; b` = chyba 2.
- if-výraz: `IF` vstupuje do FIRST(Expr). Skládá se všude, ve
  výrazové rovině je else povinný, `else if` rekurzí.
- Break-form je odchylka od LL: spolkne EOL své větve, takže
  `val abs2 = if … then⏎ 0 - x⏎ else⏎ x` končí DEDENTem bez LineEnd.
  Řešení: test na poslední token výrazu = DEDENT. Tabulka se nemění,
  odchylka třídy symtable dispatch (§5.1). Blokové větve = plný
  Block ( hodnotu určuje poslední příkaz, §2.3).
- ForStmt: jednořádková i víceřádková hlavička ( lookahead za FOR:
  `{EOL}` × `{ID}`), stráže 0..n jen ve víceřádkové. `DoBody`
  zrcadlí `Body`. Jeden generátor, žádné yield ( chyba 2, spec).
- Rozsahy: od LISTS (§18) jsou `to`/`until`/`by` binární operátory
  výrazové analýzy ( ID + strcmp). Zdroj foru je obecný výraz,
  tvar validuje §18. Meze a krok jsou výrazy typu Int. `by 0`:
  literál → chyba 10, výraz → runtime kontrola → 40.

Rozhraní s výrazovou analýzou:

| oblast |  |  |
|---|---|---|
| primární výrazy | id, literály, `()`, volání | **+ IF** (if-výraz), `List(…)` (§18) |
| operátory | `MUL DIV + - < <= > >= == != && \|\|` | **+ to/until/by/++** (§18) |
| stopping tokeny | EOL, SEMI, THEN, DO, COMMA, RPAREN | + ELSE (automaticky, neoperátor) |
| layout tokeny | nikdy uvnitř výrazu | jediná výjimka: break-form (hloubka 0) |
| after_operand | po operandu/volání/.členu | + po if-výrazu |

## 17. Rozšíření INTERPOLATEDS

§5.6: interpolátor `s`. `$id` pro proměnnou, `${výraz}` pro výraz,
`$$` vloží dolar. Víceřádková varianta `s"""…"""` podporovaná.

```ebnf
InterpString  → INTERP_START InterpContent
InterpContent → LBRACE Expr RBRACE InterpContent
              | ID InterpContent
              | INTERP_MID InterpContent
              | INTERP_END
```

- Satelit výrazové analýzy: z Program se k `InterpString` nedostanu
  derivací ( prázdné FOLLOW), volá ho greedy v operand-stavu.
- `$var` interpoluje přesně jeden identifikátor. `s"$x.y"` → `.y`
  je text. Operátory a `.člen` jen uvnitř `${…}`.
- `INTERP_START` vstupuje do FIRST výrazových pseudoterminálů:
  interp je operand jako volání či if-výraz (`s"$x".length`,
  `s"a" + s"b"`, vnořování `s"a${s"b$x"}c"`).
- Stopping: `RBRACE` automaticky (neoperátor v operátor-stavu).
- Každá vložená hodnota se převádí jako `toStr` ( spec výslovně).
  Codegen = CONCAT sekvence segmentů.
- ID v `$var` podléhá dispatchi §5.1 (0-skupinová → volá se,
  ≥1 skupina → 2, nedefinovaná → 3).

Scanner:

| token | kdy | atribut |
|---|---|---|
| `INTERP_START` | text od `s"` po `$id`, `${` nebo `"` (`$$` je text) | escap `\\` `\"` `\n` `\r` `\t`, `$$` → `$` |
| `INTERP_MID` | neprázdný text mezi interpolacemi | totéž |
| `INTERP_END` | poslední text před `"` nebo `"""` | totéž |
| `LBRACE` | `${` | žádný |
| `RBRACE` | párová `}` | žádný |

Víceřádková varianta: LF v textu povolen ( v atributu), `"` doslova.
U jednořádkové `s"…"` je LF lex chyba 1 ( i uvnitř `${…}`).

Odmítáme: `f"…"` → dva operandy → chyba 2; `$` před číslicí/klíčovým
slovem → lex 1; prázdné `${}` → chyba 2; interp jako typ → chyba 2;
holé `{` `}` → chyba 2.

## 18. Rozšíření LISTS

§5.8: seznamy `List[T]`, metody, `++`, indexování, `(1 to 6).toList`,
for přes seznam. Vyžaduje CONTROL + BOOLTHEN (máme). Subtraktivní
delta jako FUNEXP: rozsahy přejdou k výrazové analýze.

| oblast | před | po |
|---|---|---|
| rozsahy `to/until/by` | pravidla `Range`/`RangeTail`/`RangeBy` | **binární operátory výrazové analýzy** |
| zdroj generátoru | `Range` (jen rozsah) | **obecný výraz** (rozsah i `List[T]`) |
| `(1 to 6).toList` mimo for | chyba 2 | legální |
| `for i <- 1` (zdroj bez směru) | chyba 2 | tvar OK, typ rozhoduje sémantika |
| typ `List[T]` | - | `Type → LIST_TOKEN LBRACK Type RBRACK`, vnořitelný |
| `++` | - | priorita jako `+`, levá asociativita |
| `List(…)`, `List.range(…)`, `l.drop(1).take(5)`, `lst(2)` | - | zdarma (volání a `.člen` jsou postfixy) |

Tvar rozsahu validuje výrazová analýza **globálně**: §5.3 vypisuje
tři formy („from to to", „from until until", „from to to by step").
Z toho:

- jeden směr, `by` jen po `to` a jen jednou → `until … by` a
  samostatné `by` → chyba 2 (i mimo for, i v závorkách),
- žádný směr → tvar OK (seznamový výraz, typ rozhoduje sémantika),
- závorky jsou vlastní řetěz: `(1 to 5) ++ (2 to 8)` jsou dva
  validní rozsahy, `(1 until 5 by 2)` je nevalidní → chyba 2.

`LIST_TOKEN` scanner vydává přímo (klíčové slovo jako `type`;
`to`/`until`/`by` naopak zůstávají ID + strcmp). Vnořování
`List[List[Int]]` přirozeně rekurzí. `List` je built-in v symtable
jako `print`. `lst(2)` řeší dispatch §5.1: seznam + `(` = index
( typ `Int`, běhová chyba 40 mimo hranice), jiný typ + `(` = chyba 2.
Členy `head`/`tail`/`drop`/`take`/`slice`/`toString`/`toList` jsou
DOT + ID jako u VALMEM. `filter`/`takeWhile`/`dropWhile` podmiňuje
LAMBDAS, tvar `.metoda(výraz)` je legální už teď. `++` na ne-seznamu:
tvar OK, sémantika 6. `readIntMaybe`/`readDoubleMaybe` patří k
ALGEBDT (§5.5).


## 19. Rozšíření ALGEBDT

§5.5: sjednocení `T1 | T2`, aliasy `type`, součinové typy `case class`,
výraz `match` s větvemi `case vzor [if stráž] => tělo`, vestavěné
`readIntMaybe`/`readDoubleMaybe`. Čistě aditivní.

- `Type → TypeAtom TypeUnionRest` — levá faktorizace pro sjednocení.
  TypeAtom = 5 základních + `List[T]` + `ID` (alias i case class,
  symtable lookup; nedefinovaný = sémantika, ne tvar).
  TypeUnionRest = `PIPE TypeAtom TypeUnionRest | ε`, řetězitelné.
- Sjednocení povoleno kdekoli, kde je `Type` (inline param, návratový
  typ, `val x: Int | Unit`, alias). Alias může být členem dalšího
  sjednocení (§5.5: „vč. dříve zadefinovaných sjednocení typů").
- `TypeDef → TYPE ID ASSIGN Type EOL` — jen na vrcholu ( globální
  rozsah).
- `CaseClassDef → CASE CLASS ID LPAREN ParamList RPAREN EOL` — sdílí
  ParamList s `def` (koncová čárka s EOL-flagem zdarma). Atributy
  nemodifikovatelné veřejné. Tělo, metody, copy, dědičnost NE
  ( §5.5 výslovně).
- Match je výraz a postfix na operandu: `value match …`. Řeší ho kód,
  ne pravidla (vzor break-form if-výrazu §16). Tělo větve = tentýž
  tvar jako větev if-výrazu. Vyčerpávajícíost je sémantika
  ( „postačí… vždy vyžaduje větev case _").
- `(x match …) + 1` uvnitř závorek scanner neumí (EOL/INDENT/DEDENT
  se uvnitř závorek nemitují), tento tvar není podporovaný.
- `readIntMaybe`/`readDoubleMaybe`: návrat `Int | Unit` /
  `Double | Unit`; chybný vstup → `()`.

```ebnf
Pattern    → INT_NUM | DOUBLE_NUM | STRING_LITERAL
           | TRUE_TOKEN | FALSE_TOKEN
           | UNDERSCORE                     (* _ *)
           | LPAREN RPAREN                  (* () = Unit literál *)
           | MINUS PatNeg                   (* záporný literál *)
           | ID PatTail                     (* Point(x, y) nebo n: Int *)

PatTail    → COLON TypeAtom                 (* n: Int *)
           | LPAREN PatList RPAREN          (* Point(a, b) *)
           (* BEZ ε — case x => = chyba 2, §5.5 nezahrnuje vazebný vzor *)

PatNeg     → INT_NUM | DOUBLE_NUM

PatList    → PatArg PatRest
           | ε

PatRest    → COMMA PatArg PatRest
           | ε

PatArg     → ID | INT_NUM | DOUBLE_NUM | STRING_LITERAL
           | TRUE_TOKEN | FALSE_TOKEN | UNDERSCORE
           | MINUS PatNeg
           (* PLOCHÝ — žádná rekurze do Pattern *)
```

- `PatTail` bez ε: `case x =>` není podporovaný vzor ( §5.5).
- `PatTail → COLON TypeAtom` (ne Type): vnořené typové vzory
  (`case x: Int | Unit =>`) nejsou podporované ( §5.5).
- PatArg plochý: vnořené konstruktory (`Point(Point(x))`) ne.
- `()` jako Unit literál: `readIntMaybe` vrací `Int | Unit`,
  `case () =>` je potřeba pro rozlišení chybného vstupu.
- Záporné literály přes `MINUS PatNeg` (faktorizace MINUS × MINUS).

Sémantika: hodnota `T1 | T2` = hodnota členského typu; přístup
k operacím konkrétního členu až po rozlišení match-vzorem. Case class:
`Point(2, 3)` = volání (FUNEXP), `point.x` = člen (VALMEM tvar).
Codegen: match = kaskáda if shora dolů; součin = struktura v IFJcode26.
`List[Int] | Unit` (LIST_TOKEN v TypeAtom), alias jako typ, case class
jméno v typové pozici — vše zdarma od předchozích rozšíření.

