# LL tabulka (v7)

Ověřeno skriptem `ll_check.py` (0 kolizí). Kombinace neterminál × token, která v tabulce není, = prázdná buňka = syntaktická chyba 2.

| Neterminál | Tokeny | Pravidlo |
|---|---|---|
| `AfterCommaArg` | `DOUBLE_NUM`, `ID`, `INT_NUM`, `LPAREN`, `STRING_LITERAL` | `Term ArgRest` |
| `AfterCommaArg` | `RPAREN` | `ε` |
| `AfterCommaParam` | `ID` | `Param ParamRest` |
| `AfterCommaParam` | `RPAREN` | `ε` |
| `ArgList` | `DOUBLE_NUM`, `ID`, `INT_NUM`, `LPAREN`, `STRING_LITERAL` | `Term ArgRest` |
| `ArgList` | `RPAREN` | `ε` |
| `ArgRest` | `COMMA` | `COMMA AfterCommaArg` |
| `ArgRest` | `RPAREN` | `ε` |
| `Block` | `DOUBLE_NUM`, `ID`, `IF`, `INT_NUM`, `LPAREN`, `MINUS`, `PLUS`, `RETURN`, `STRING_LITERAL`, `VAL`, `VAR`, `WHILE` | `BlockItem StmtRest` |
| `BlockItem` | `DOUBLE_NUM`, `ID`, `INT_NUM`, `LPAREN`, `MINUS`, `PLUS`, `RETURN`, `STRING_LITERAL`, `VAL`, `VAR` | `SimpleStmt EndSimple` |
| `BlockItem` | `IF` | `IfStmt` |
| `BlockItem` | `WHILE` | `WhileStmt` |
| `Body` | `DOUBLE_NUM`, `ID`, `INT_NUM`, `LPAREN`, `MINUS`, `PLUS`, `STRING_LITERAL` | `Rhs EOL` |
| `Body` | `EOL` | `EOL INDENT Block DEDENT` |
| `EndSimple` | `EOL` | `EOL` |
| `EndSimple` | `SEMICOLON` | `SEMICOLON` |
| `FuncDef` | `DEF` | `DEF ID Params RetType ASSIGN Body` |
| `FuncDef` | `AT` | `AT ID OptEol DEF ID Params RetType ASSIGN Body` |
| `FuncDefs` | `EOL` | `EOL FuncDefs` |
| `FuncDefs` | `AT`, `DEF` | `FuncDef FuncDefs` |
| `FuncDefs` | `EOF` | `ε` |
| `IdTail` | `LPAREN` | `LPAREN ArgList RPAREN` |
| `IdTail` | `DIV`, `EOL`, `EQ`, `GE`, `GT`, `LE`, `LT`, `MINUS`, `MUL`, `NEQ`, `PLUS`, `SEMICOLON` | `ExprFromId` |
| `IfStmt` | `IF` | `IF Expr THEN EOL INDENT Block DEDENT ELSE EOL INDENT Block DEDENT` |
| `OptEol` | `EOL` | `EOL` |
| `OptEol` | `DEF` | `ε` |
| `Param` | `ID` | `ID COLON Type` |
| `ParamList` | `ID` | `Param ParamRest` |
| `ParamList` | `RPAREN` | `ε` |
| `ParamRest` | `COMMA` | `COMMA AfterCommaParam` |
| `ParamRest` | `RPAREN` | `ε` |
| `Params` | `LPAREN` | `LPAREN ParamList RPAREN` |
| `Program` | `IMPORT` | `Prolog FuncDefs EOF` |
| `Prolog` | `IMPORT` | `IMPORT ID DOT MUL EOL` |
| `RetType` | `COLON` | `COLON Type` |
| `RetType` | `ASSIGN` | `ε` |
| `ReturnStmt` | `RETURN` | `RETURN RhsOpt` |
| `Rhs` | `ID` | `ID IdTail` |
| `Rhs` | `DOUBLE_NUM`, `INT_NUM`, `LPAREN`, `MINUS`, `PLUS`, `STRING_LITERAL` | `ExprNoId` |
| `RhsOpt` | `DOUBLE_NUM`, `ID`, `INT_NUM`, `LPAREN`, `MINUS`, `PLUS`, `STRING_LITERAL` | `Rhs` |
| `RhsOpt` | `EOL`, `SEMICOLON` | `ε` |
| `SimpleStmt` | `VAL` | `ValStmt` |
| `SimpleStmt` | `VAR` | `VarStmt` |
| `SimpleStmt` | `RETURN` | `ReturnStmt` |
| `SimpleStmt` | `ID` | `ID StmtIdTail` |
| `SimpleStmt` | `DOUBLE_NUM`, `INT_NUM`, `LPAREN`, `MINUS`, `PLUS`, `STRING_LITERAL` | `ExprNoId` |
| `StmtIdTail` | `ASSIGN` | `ASSIGN Rhs` |
| `StmtIdTail` | `DIV`, `EOL`, `EQ`, `GE`, `GT`, `LE`, `LPAREN`, `LT`, `MINUS`, `MUL`, `NEQ`, `PLUS`, `SEMICOLON` | `IdTail` |
| `StmtRest` | `EOL` | `EOL StmtRest` |
| `StmtRest` | `DOUBLE_NUM`, `ID`, `IF`, `INT_NUM`, `LPAREN`, `MINUS`, `PLUS`, `RETURN`, `STRING_LITERAL`, `VAL`, `VAR`, `WHILE` | `BlockItem StmtRest` |
| `StmtRest` | `DEDENT` | `ε` |
| `Term` | `ID` | `ID` |
| `Term` | `INT_NUM` | `INT_NUM` |
| `Term` | `DOUBLE_NUM` | `DOUBLE_NUM` |
| `Term` | `STRING_LITERAL` | `STRING_LITERAL` |
| `Term` | `LPAREN` | `LPAREN RPAREN` |
| `Type` | `INT_TOKEN` | `INT_TOKEN` |
| `Type` | `DOUBLE_TOKEN` | `DOUBLE_TOKEN` |
| `Type` | `STRING_TOKEN` | `STRING_TOKEN` |
| `Type` | `UNIT_TOKEN` | `UNIT_TOKEN` |
| `ValStmt` | `VAL` | `VAL ID ValVarTail` |
| `ValVarTail` | `COLON` | `COLON Type ASSIGN Rhs` |
| `ValVarTail` | `ASSIGN` | `ASSIGN Rhs` |
| `VarStmt` | `VAR` | `VAR ID ValVarTail` |
| `WhileStmt` | `WHILE` | `WHILE Expr DO EOL INDENT Block DEDENT` |
