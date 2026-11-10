# IFJ26 LL gramatika (v8)

Verze: v8, základní zadání + rozšíření BOOLTHEN.

Otázky, které tato verze řešila, a jejich rozhodnutí (zdůvodnění v §11 a §13):

| otázka | rozhodnutí |
|---|---|
| `if` bez `else`, `else if`, Boolean literály a `!` | BOOLTHEN (§13) |

Verifikace: `ll_check.py`, 86 tokenových streamů. Ověřuje bezkoliznost
LL tabulky (FIRST/FOLLOW, SELECT, 0 kolizí) a přijetí nebo odmítnutí vstupů
tabulkovým parserem (86/86, §9). Rozsah záruky: bezkoliznost platí pro
modelované rozhraní (pseudoterminály mají FIRST ručně, výrazová analýza je
aproximována automatem operandů a operátorů). Skript neověřuje sémantiku ani
úplnost IFJ26; výrazy v celé šíři řeší precedenční tabulka.

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
- Neterminály CamelCase; v C funkce `parse_xxx()` ve snake_case.
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
         | ε                              (* Predikt: {EOL} | {DEF, AT} | {EOF} *)
FuncDef  → DEF ID Params RetType ASSIGN Body
         | AT ID OptEol DEF ID Params RetType ASSIGN Body
                                        (* v kódu: attr(ID za AT)=="main" *)
OptEol   → EOL
         | ε                             (* Predikt: {EOL} | FOLLOW = {DEF} *)
```

Řešení:

- Na vrchní úrovni jsou jen definice funkcí. Příkaz po definicích skončí
  na prázdné buňce `FuncDefs × VAL`, chybou 2.
- Prázdné řádky konzumuje seznam (`FuncDefs → EOL FuncDefs`), ne okolí.
  Specifikace §1.2 prázdné řádky ignoruje, takže mají jediné místo,
  řeší jediné pravidlo. Stejný vzor jako `StmtRest` v §3.
- `OptEol`: anotace @main může být na samostatném řádku i na stejném
  řádku s `def`. Specifikace §2: „Anotace může být na samostatném řádku
  nebo na stejném řádku s klíčovým slovem def.“ Znak @ se v jiném
  kontextu nepoužívá, mezi ním a `main` nesmí být žádný znak.
- Kontrola `ifj26` a `main` je kontrola atributu tokenu v kódu
  pomocí `strcmp` na atributu
tokenu. Gramatika neumí
  vyjádřit „identifikátor s konkrétní hodnotou“.

## 2. Hlavička funkce

```ebnf
Params          → LPAREN ParamList RPAREN
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
                | BOOLEAN_TOKEN                 (* v8 BOOLTHEN *)
Body            → Rhs EOL                       (* jednořádkové tělo *)
                | EOL INDENT Block DEDENT        (* blokové tělo *)
```

Řešení:

- Levá faktorizace: po `ParamRest`. Po `COMMA` rozhoduje
  `AfterCommaParam` jedním lookaheadem: `ID` znamená další parametr,
  `RPAREN` znamená konec seznamu. Dvě pravidla začínající stejným
  tokenem by v LL kolidovala.
- Koncová čárka (§1.2): „Za poslední položkou neprázdného seznamu
  parametrů … je povolena koncová čárka, pouze pokud je následujícím
  tokenem uzavírací závorka a mezi čárkou a touto závorkou je znak
  konce řádku.“ Řeší ji větev `AfterCommaParam → ε`: `RPAREN`
  konzumuje až nadřazené `Params` (parser v ε-větvi ověří flag ze scanneru (konec řádku před touto
  `)`). Bez flagu jde o chybu 2.
- Návratový typ je syntakticky nepovinný (`RetType → ε`, FOLLOW
  = `{ASSIGN}`). Povinnost při rekurzi nebo výskytu `return`
  je sémantická kontrola (typová chyba 6), ne tvar.
- Tělo má dvě formy (§2.3). Rozlišuje je jeden lookahead: začátek
  výrazu nebo `EOL`.

## 3. Blok a seznam příkazů

Ukončovatele určují dvě kategorie příkazů:

| kategorie | příkazy | konec |
|---|---|---|
| jednořádkové | val, var, return, přiřazení, volání, výraz | povinný `EndSimple` (EOL nebo `;`) |
| víceřádkové | if, while | DEDENT svého bloku, nic extra za nimi |

```ebnf
Block      → BlockItem StmtRest              (* neprázdný, §2.1 *)
BlockItem  → SimpleStmt EndSimple
           | IfStmt
           | WhileStmt
StmtRest   → EOL StmtRest                    (* prázdné řádky / EOL za ; *)
           | BlockItem StmtRest               (* další příkaz *)
           | ε                                 (* FOLLOW = {DEDENT} *)
EndSimple  → EOL | SEMICOLON                  (* povinný *)
```

Řešení:

- Ukončovatel patří příkazu. Kdyby EOL konzumovalo okolí, bylo by přijato
  `val a = 1 val b = 2` bez oddělovače. S povinným `EndSimple`
  skončí chybou 2 (prázdná buňka `EndSimple × VAL`, chybou 2.
- Volné řádky konzumuje seznam. Za `;` může přijít EOL
  a před DEDENT mohou být prázdné řádky. `StmtRest → EOL StmtRest`
  konzumuje EOLy po jednom a vždy znovu rozhoduje. 
- Blok je neprázdný (§2.1: „Blok musí mít vždy tělo“); prázdné
  tělo se píše literálem `()`.
- Víceřádkové příkazy nemají za sebou ukončovatel: EOL patří
  poslednímu příkazu uvnitř bloku, DEDENT zavírá konstrukci.

Rohové případy (ověřeno trasováním, §9):

| | stream | výsledek |
|---|---|---|
| A | `VAL ID ASSIGN INT EOL VAL …` | OK, `EndSimple=EOL`, další BlockItem |
| B | `… INT SEMI VAL …` | OK, `EndSimple=SEMI` |
| C | `… INT SEMI EOL DEDENT` | OK, `StmtRest→EOL StmtRest→ε` |
| D | `… DEDENT DEDENT …` | OK, každý DEDENT patří své konstrukci |
| E | `… INT EOL DEDENT` | OK, `StmtRest→ε` |
| F | `… INT VAL …` | chyba 2, `EndSimple × VAL`, prázdná buňka |
| G | `… DEDENT ELSE …` | OK, `StmtRest→ε`, IfStmt pokračuje ELSE |

## 4. Příkazy

```ebnf
SimpleStmt → ValStmt | VarStmt | ReturnStmt
           | ID StmtIdTail                   (* přiřazení / volání / výraz od id *)
           | ExprNoId                        (* výraz nezačínající id *)
ValStmt    → VAL ID ValVarTail
VarStmt    → VAR ID ValVarTail
ValVarTail → COLON Type ASSIGN Rhs            (* s typem, jeden logický řádek *)
           | ASSIGN Rhs                       (* bez typu *)
IfStmt     → IF Expr THEN EOL INDENT Block DEDENT IfTail
IfTail     → ELSE IfBranch                   (* v8 BOOLTHEN: else volitelné *)
           | ε
IfBranch   → EOL INDENT Block DEDENT IfTail
           | IfStmt                          (* else if - celé if rekurzivně *)
WhileStmt  → WHILE Expr DO EOL INDENT Block DEDENT
ReturnStmt → RETURN RhsOpt                   (* výraz i volání *)
RhsOpt     → Rhs
           | ε                                (* holé return, §11.1; FOLLOW = {EOL, SEMICOLON} *)
```

Řešení:

- Kolize `id` na úrovni příkazu je čistě syntaktická. Příkaz
  začínající identifikátorem rozhoduje jeden lookahead za `id`: `=` znamená přiřazení,
  `(` volání, ostatní tokeny znamenají výraz. Tabulka symbolů v této vrstvě
  není potřeba.
- `val` a `var` sdílejí `ValVarTail`. (liší se prvním tokenem) Inicializace je vždy povinná (§2.2); `var x: Int` bez `=`
  skončí chybou 2 (prázdná buňka `ASSIGN × EOL`, chybou 2.
- `if` a `while`: po hlavičce následuje EOL a odsazený blok (§1.2).
  `else` je od v8 volitelný (BOOLTHEN, §13.3) a `else if` je rekurzivní
  `IfStmt` (§13.4). Za `DEDENT` větve then následuje `ELSE` přímo.
- `return` přebírá výraz nebo volání (`RhsOpt`), viz §4.2.

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

### 4.2 `return` umí i volání

Učitelova ukázka pro základní zadání (`strings.scala`, ř. 12 a 37)
obsahuje `return subStr(text, 0, last)`. Čistý `Expr` by ji odmítl,
a ukázky učitele jsou první, na čem se testuje. Proto
`ReturnStmt → RETURN RhsOpt` (§2.3: „Příkaz návratu z funkce
return výraz-nebo-volání“).

- Volání v podmínce naopak zakázané: `IfStmt` a `WhileStmt` mají čisté
  `Expr`. Všechny ukázky mají čisté relační výrazy, §2.4 „podmínka musí
  být relační výraz“ a §3 (volání nejsou výrazy). FUNEXP je až rozšíření.
- Symtable dispatch (§5.2) platí i pro `return`: `return f` s f funkcí
  znamená chybu 2 (funkce není hodnota), `return f(1)` s f proměnnou
  chybu 2 (za proměnnou nesmí být závorka), nedefinované id chybu 3.
- Holé `return`: ε-větev `RhsOpt` zůstává, rozhodnuto připouštět (§11.1).

## 5. Výraz-nebo-volání: Rhs a kolize id

```ebnf
Rhs        → ID IdTail                       (* výraz-nebo-volání: za =, za return, v těle *)
           | ExprNoId                        (* literál / ( / unární ±: rovnou výrazová analýza *)
IdTail     → LPAREN ArgList RPAREN           (* volání id(args) *)
           | ExprFromId                       (* výraz od uloženého id *)
StmtIdTail → ASSIGN Rhs                      (* přiřazení, jen na úrovni příkazu *)
           | IdTail

Expr       → (* výraz od aktuálního tokenu *)
ExprNoId   → (* totéž; volající zaručuje, že začátek není ID *)
ExprFromId → (* výraz od už přečteného tokenu ID *)
```

`Expr`, `ExprNoId`, `ExprFromId` nemají pravidla; jsou to styčné body
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
nemění, mění se jen akce v `parse_rhs()` a `parse_stmt_id_tail()`
(volba větve podle symtable a lookaheadu, stejná technika jako
`strcmp` u `ifj26` a `main`).

| `symtable_kind(id)` | další token | akce |
|---|---|---|
| funkce | `(` | volání (`IdTail → LPAREN ArgList RPAREN`) |
| funkce | jiný | chyba 2 (funkce není hodnota v základu) |
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
ArgList       → Term ArgRest
              | ε                            (* FIRST(Term) | {RPAREN} *)
ArgRest       → COMMA AfterCommaArg
              | ε
AfterCommaArg → Term ArgRest
              | ε                            (* koncová čárka: jen s EOL-flagem *)
Term          → ID | INT_NUM | DOUBLE_NUM | STRING_LITERAL
              | TRUE_TOKEN | FALSE_TOKEN      (* v8 BOOLTHEN: literály §5.2 *)
              | LPAREN RPAREN                (* () = literál Unit *)
```

Řešení:

- Argumenty jsou v základu pouze termy. §2.3: „seznam argumentů je
  tvořen pouze termy, tedy literály nebo identifikátory proměnných
  (ne výrazy)“. Plný výraz jako argument je až FUNEXP.
- `()` je literál Unit (§1.1: „Literál hodnoty typu Unit se zapisuje
  ()“), proto je legální například `print(())`.
- Koncová čárka: tentýž vzor jako u parametrů v §2, včetně kontroly
  EOL-flagu v ε-větvi `AfterCommaArg`.

## 7. FIRST / FOLLOW

| neterminál | FIRST | FOLLOW | ε? |
|---|---|---|---|
| Program | `IMPORT` | – | ne |
| Prolog | `IMPORT` | `AT DEF EOL EOF` | ne |
| FuncDefs | `AT DEF EOL` | `EOF` | ano |
| FuncDef | `AT DEF` | `AT DEF EOL EOF` | ne |
| OptEol | `EOL` | `DEF` | ano |
| Params | `LPAREN` | `ASSIGN COLON` | ne |
| ParamList | `ID` | `RPAREN` | ano |
| ParamRest | `COMMA` | `RPAREN` | ano |
| AfterCommaParam | `ID` | `RPAREN` | ano |
| Param | `ID` | `COMMA RPAREN` | ne |
| RetType | `COLON` | `ASSIGN` | ano |
| Type | `INT_TOKEN DOUBLE_TOKEN STRING_TOKEN UNIT_TOKEN BOOLEAN_TOKEN` | `ASSIGN COMMA RPAREN` | ne |
| IfTail | `ELSE` | stmt-starty, `DEDENT EOL` | ano |
| IfBranch | `EOL IF` | stmt-starty, `DEDENT EOL` | ne |
| Body | `EOL ID INT_NUM DOUBLE_NUM STRING_LITERAL LPAREN PLUS MINUS` | `AT DEF EOL EOF` | ne |
| Block | `VAL VAR IF WHILE RETURN ID INT_NUM DOUBLE_NUM STRING_LITERAL LPAREN PLUS MINUS` | `DEDENT` | ne |
| BlockItem | (též jako Block) | stmt-starty, `DEDENT EOL` | ne |
| StmtRest | `EOL`, stmt-starty | `DEDENT` | ano |
| EndSimple | `EOL SEMICOLON` | stmt-starty, `DEDENT EOL` | ne |
| SimpleStmt | `VAL VAR RETURN ID INT_NUM DOUBLE_NUM STRING_LITERAL LPAREN PLUS MINUS TRUE FALSE NOT` | `EOL SEMICOLON` | ne |
| ValStmt / VarStmt | `VAL` / `VAR` | `EOL SEMICOLON` | ne |
| ValVarTail | `ASSIGN COLON` | `EOL SEMICOLON` | ne |
| IfStmt / WhileStmt | `IF` / `WHILE` | stmt-starty, `DEDENT EOL` | ne |
| ReturnStmt | `RETURN` | `EOL SEMICOLON` | ne |
| RhsOpt | `ID INT_NUM DOUBLE_NUM STRING_LITERAL LPAREN PLUS MINUS` | `EOL SEMICOLON` | ano |
| Rhs | `ID INT_NUM DOUBLE_NUM STRING_LITERAL LPAREN PLUS MINUS TRUE FALSE NOT` | `EOL SEMICOLON` | ne |
| IdTail | `LPAREN`, operátory, `EOL SEMICOLON` | `EOL SEMICOLON` | ne |
| StmtIdTail | `ASSIGN LPAREN`, operátory, `EOL SEMICOLON` | `EOL SEMICOLON` | ne |
| ArgList | `ID INT_NUM DOUBLE_NUM STRING_LITERAL LPAREN` | `RPAREN` | ano |
| ArgRest | `COMMA` | `RPAREN` | ano |
| AfterCommaArg | `ID INT_NUM DOUBLE_NUM STRING_LITERAL LPAREN` | `RPAREN` | ano |
| Term | `ID INT_NUM DOUBLE_NUM STRING_LITERAL LPAREN TRUE FALSE` | `COMMA RPAREN` | ne |

Pseudoterminály (interface, bez pravidel): `FIRST(Expr)` = FIRST(Rhs);
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
příkaz od id i od literálu; unární minus; `()` jako příkaz i tělo;
podmínka v závorkách; while; return s výrazem, voláním i holý;
jednořádkové tělo voláním; @main s blokem i na samostatném řádku;
`print(())`; prázdné řádky mezi definicemi i po importu; jen import
bez definic; faktoriál; celý program (add + @main).

Nelegální vstupy, všechny končí chybou 2:

| vstup | kde to skončí |
|---|---|
| `val a = 1 val b = 2` | `EndSimple × VAL`, prázdná buňka |
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
| `x = a + f(1)` | výrazová analýza se zastaví před `(`, `EndSimple × LPAREN` |
| `x = f(1) + 2` | volání hotové, `EndSimple × PLUS` |
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
   „Samostatný return vrací () a je povolen pouze ve funkci
   s explicitním návratovým typem Unit.“ Syntakticky legální;
   navazující sémantická kontrola: jen explicitní Unit, porušení
   znamená typovou chybu 6. Doplňkové pravidlo tamtéž: funkce
   s explicitním Unit vyhodnotí návratovou hodnotu a zahodí ji
   (i u return), sémantika tedy nesmí mechanicky vyžadovat typ Unit
   u vraceného výrazu.
2. Volání v podmínce a v returnu: `return subStr(text, 0, last)`
   v `strings.scala` (ř. 12 a 37) znamená, že return umí volání
   (`ReturnStmt → RETURN RhsOpt`). Volání v podmínce zakázané:
   čisté `Expr`, §2.4 a §3.
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
6. `;` mezi definicemi a za jednořádkovým tělem: odmítáme.
   §1.2 dává středník jako oddělovač příkazů; §2 říká, že za prologem
   jsou definice funkcí, a definice nejsou příkazy. V žádné ukázce
   učitelů se středník nevyskytuje. Tolerantní varianta by byla
   `Body → Rhs EndSimple`, jedno pravidlo.
7. Typ a `=` na různých řádcích: chyba 2. Zdůvodnění v §4.1
   (logický řádek, jedna forma val, směr platnosti Scaly, asymetrie
   odsazení).

## 12. Mapování do parser.c

| neterminál | funkce | poznámky ke kódu |
|---|---|---|
| Program | `parse_program()` | |
| Prolog | `parse_prolog()` | po matchi ID: `strcmp(attr,"ifj26")`, jinak chyba 2 |
| FuncDefs | `parse_func_defs()` | smyčka: EOL / DEF / AT / EOF |
| FuncDef | `parse_func_def()` | větev AT: `strcmp(attr,"main")` |
| OptEol | inline | `if (cur==EOL) advance();` |
| Params / ParamList / ParamRest / AfterCommaParam | `parse_params()` a další | ε-větev AfterCommaParam: check EOL-flagu |
| Param | `parse_param()` | |
| RetType | `parse_ret_type()` | |
| Type | `parse_type()` | switch přes 4 tokeny |
| Body | `parse_body()` | |
| Block | `parse_block()` | |
| BlockItem | `parse_block_item()` | kategorie: jednoduchý → SimpleStmt + EndSimple; složený → If/While |
| StmtRest | `parse_stmt_rest()` | smyčka: EOL / další / konec |
| EndSimple | inline `expect(EOL|SEMICOLON)` | |
| SimpleStmt | `parse_simple_stmt()` | switch přes první token |
| ValStmt / VarStmt | `parse_val_stmt()` / `parse_var_stmt()` | společný `parse_val_var_tail()` |
| ValVarTail | `parse_val_var_tail()` | jednořádkové (§4.1) |
| IfStmt / WhileStmt | `parse_if_stmt()` / `parse_while_stmt()` | |
| IfTail / IfBranch | `parse_if_tail()` / `parse_if_branch()` | else-if = rekurzivní `parse_if_stmt()` |
| ReturnStmt | `parse_return_stmt()` | ε-větev `RhsOpt` = holý return |
| RhsOpt | inline | |
| Rhs | `parse_rhs()` | ID → dispatch symtable (§5.1) → parse_id_tail() / `parse_expression_from()`; jinak `parse_expression()` |
| StmtIdTail | `parse_stmt_id_tail()` | ASSIGN → parse_rhs(); jinak dispatch (§5.1) |
| IdTail | `parse_id_tail()` | LPAREN → args; jinak `parse_expression_from(saved)` |
| ArgList / ArgRest / AfterCommaArg | `parse_args()` a další | flag u AfterCommaArg |
| Term | `parse_term()` | včetně `()` |
| Expr / ExprNoId / ExprFromId | precedenční analýza | `parse_expression()`, `parse_expression_from(tok)`; stopping: EOL, SEMICOLON, THEN, DO |

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

`IfTail → ELSE IfBranch | ε`. Predikt ε-větve je FOLLOW(IfTail) =
stmt-starty ∪ {EOL, DEDENT}; větev s ELSE má Predikt {ELSE}. Množiny jsou
disjunktní - `ELSE` není začátek příkazu ani ukončovač, takže parser
na DEDENT či začátku dalšího příkazu správně pozná konec ifu.

Otestováno v obou polohách: if bez else následovaný dalším příkazem
(StmtRest pokračuje) i jako poslední příkaz bloku (StmtRest končí
na DEDENT).

### 13.4 `else if` a dangling else

`IfBranch → EOL INDENT Block DEDENT IfTail | IfStmt`. Za `else if` stojí
celé if znovu - rekurzivní použití `IfStmt`, ne nová větev. Rozlišení
jedním lookaheadem: `{EOL}` × `{IF}`, čisté.

„Zarovnáno s if" (§5.2) je pravidlo scanneru, v LL tabulce neviditelné -
vidíme jen jeho důsledek, počty DEDENTů. Méně odsazený `else if` proto
uvízne stejně jako `else` bez ničeho (prázdná buňka `IfBranch × DEDENT`,
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

„Libovolný výraz typu Boolean" (§5.2) místo „relační výraz" (§2.4) -
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
