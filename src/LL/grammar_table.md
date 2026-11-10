# LL tabulka (v10)

Ověřeno skriptem `ll_check.py` (0 kolizí). Kombinace neterminál × token, která v tabulce není, = prázdná buňka = syntaktická chyba 2.

| Neterminál | Tokeny | Pravidlo |
|---|---|---|
| `AfterCommaParam` | `ID` | `Param ParamRest` |
| `AfterCommaParam` | `RPAREN` | `ε` |
| `Block` | `DOUBLE_NUM`, `FALSE_TOKEN`, `ID`, `IF`, `INT_NUM`, `LPAREN`, `RETURN`, `STRING_LITERAL`, `TRUE_TOKEN`, `VAL`, `VAR`, `WHILE` | `BlockItem StmtRest` |
| `BlockItem` | `DOUBLE_NUM`, `FALSE_TOKEN`, `ID`, `INT_NUM`, `LPAREN`, `RETURN`, `STRING_LITERAL`, `TRUE_TOKEN`, `VAL`, `VAR` | `SimpleStmt LineEnd` |
| `BlockItem` | `IF` | `IfStmt` |
| `BlockItem` | `WHILE` | `WhileStmt` |
| `Body` | `DOUBLE_NUM`, `FALSE_TOKEN`, `ID`, `INT_NUM`, `LPAREN`, `MINUS`, `NOT_TOKEN`, `PLUS`, `STRING_LITERAL`, `TRUE_TOKEN` | `Rhs EOL` |
| `Body` | `EOL` | `EOL INDENT Block DEDENT` |
| `ExtDefs` | `EOL` | `EOL ExtDefs` |
| `ExtDefs` | `AT`, `DEF` | `FuncDef ExtDefs` |
| `ExtDefs` | `DEDENT` | `ε` |
| `ExtensionDef` | `EXTENSION` | `EXTENSION LPAREN Param RPAREN EOL INDENT ExtDefs DEDENT` |
| `FuncDef` | `DEF` | `DEF ID Params RetType ASSIGN Body` |
| `FuncDef` | `AT` | `AT ID OptEol DEF ID Params RetType ASSIGN Body` |
| `FuncDefs` | `EOL` | `EOL FuncDefs` |
| `FuncDefs` | `AT`, `DEF` | `FuncDef FuncDefs` |
| `FuncDefs` | `EXTENSION` | `ExtensionDef FuncDefs` |
| `FuncDefs` | `EOF` | `ε` |
| `IfBranch` | `EOL` | `EOL INDENT Block DEDENT` |
| `IfBranch` | `IF` | `IfStmt` |
| `IfStmt` | `IF` | `IF Expr THEN EOL INDENT Block DEDENT IfTail` |
| `IfTail` | `ELSE` | `ELSE IfBranch` |
| `IfTail` | `DEDENT`, `DOUBLE_NUM`, `EOL`, `FALSE_TOKEN`, `ID`, `IF`, `INT_NUM`, `LPAREN`, `RETURN`, `STRING_LITERAL`, `TRUE_TOKEN`, `VAL`, `VAR`, `WHILE` | `ε` |
| `LineEnd` | `EOL` | `EOL` |
| `LineEnd` | `SEMICOLON` | `SEMICOLON MidItem` |
| `MidItem` | `DOUBLE_NUM`, `FALSE_TOKEN`, `ID`, `INT_NUM`, `LPAREN`, `MINUS`, `NOT_TOKEN`, `PLUS`, `RETURN`, `STRING_LITERAL`, `TRUE_TOKEN`, `VAL`, `VAR` | `MidSimple LineEnd` |
| `MidItem` | `IF` | `IfStmt` |
| `MidItem` | `WHILE` | `WhileStmt` |
| `MidItem` | `EOL` | `EOL` |
| `MidSimple` | `VAL` | `ValStmt` |
| `MidSimple` | `VAR` | `VarStmt` |
| `MidSimple` | `RETURN` | `ReturnStmt` |
| `MidSimple` | `ID` | `ID StmtIdTail` |
| `MidSimple` | `DOUBLE_NUM`, `FALSE_TOKEN`, `INT_NUM`, `LPAREN`, `MINUS`, `NOT_TOKEN`, `PLUS`, `STRING_LITERAL`, `TRUE_TOKEN` | `ExprNoId` |
| `OptEol` | `EOL` | `EOL` |
| `OptEol` | `DEF` | `ε` |
| `Param` | `ID` | `ID COLON Type` |
| `ParamGroup` | `LPAREN` | `LPAREN ParamList RPAREN` |
| `ParamList` | `ID` | `Param ParamRest` |
| `ParamList` | `RPAREN` | `ε` |
| `ParamRest` | `COMMA` | `COMMA AfterCommaParam` |
| `ParamRest` | `RPAREN` | `ε` |
| `Params` | `LPAREN` | `ParamGroup Params` |
| `Params` | `ASSIGN`, `COLON` | `ε` |
| `Program` | `IMPORT` | `Prolog FuncDefs EOF` |
| `Prolog` | `IMPORT` | `IMPORT ID DOT MUL EOL` |
| `RetType` | `COLON` | `COLON Type` |
| `RetType` | `ASSIGN` | `ε` |
| `ReturnStmt` | `RETURN` | `RETURN RhsOpt` |
| `RhsOpt` | `DOUBLE_NUM`, `FALSE_TOKEN`, `ID`, `INT_NUM`, `LPAREN`, `MINUS`, `NOT_TOKEN`, `PLUS`, `STRING_LITERAL`, `TRUE_TOKEN` | `Rhs` |
| `RhsOpt` | `EOL`, `SEMICOLON` | `ε` |
| `SimpleStmt` | `VAL` | `ValStmt` |
| `SimpleStmt` | `VAR` | `VarStmt` |
| `SimpleStmt` | `RETURN` | `ReturnStmt` |
| `SimpleStmt` | `ID` | `ID StmtIdTail` |
| `SimpleStmt` | `DOUBLE_NUM`, `FALSE_TOKEN`, `INT_NUM`, `LPAREN`, `STRING_LITERAL`, `TRUE_TOKEN` | `ExprStmt` |
| `StmtIdTail` | `ASSIGN` | `ASSIGN Rhs` |
| `StmtIdTail` | `AND_TOKEN`, `DIV`, `DOT`, `EOL`, `EQ`, `GE`, `GT`, `LE`, `LPAREN`, `LT`, `MINUS`, `MUL`, `NEQ`, `OR_TOKEN`, `PLUS`, `SEMICOLON` | `ExprFromId` |
| `StmtRest` | `EOL` | `EOL StmtRest` |
| `StmtRest` | `DOUBLE_NUM`, `FALSE_TOKEN`, `ID`, `IF`, `INT_NUM`, `LPAREN`, `RETURN`, `STRING_LITERAL`, `TRUE_TOKEN`, `VAL`, `VAR`, `WHILE` | `BlockItem StmtRest` |
| `StmtRest` | `DEDENT` | `ε` |
| `Type` | `INT_TOKEN` | `INT_TOKEN` |
| `Type` | `DOUBLE_TOKEN` | `DOUBLE_TOKEN` |
| `Type` | `STRING_TOKEN` | `STRING_TOKEN` |
| `Type` | `UNIT_TOKEN` | `UNIT_TOKEN` |
| `Type` | `BOOLEAN_TOKEN` | `BOOLEAN_TOKEN` |
| `ValStmt` | `VAL` | `VAL ID ValVarTail` |
| `ValVarTail` | `COLON` | `COLON Type ASSIGN Rhs` |
| `ValVarTail` | `ASSIGN` | `ASSIGN Rhs` |
| `VarStmt` | `VAR` | `VAR ID ValVarTail` |
| `WhileStmt` | `WHILE` | `WHILE Expr DO EOL INDENT Block DEDENT` |
