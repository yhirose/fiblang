FibLang
=======

A Programming Language just for writing [Fibonacci number](https://en.wikipedia.org/wiki/Fibonacci_number) program. :)

```
def fib(x)
  x < 2 ? 1 : fib(x - 2) + fib(x - 1)

for n from 1 to 30
  puts(fib(n))
```

Build and Run demo
------------------

```bash
> make
...
./fib fib.fib
1
2
3
5
8
13
21
34
55
89
144
233
...
```

PEG grammar
-----------

```peg
# Syntax
START             ← STATEMENTS
STATEMENTS        ← (DEFINITION / EXPRESSION)*
DEFINITION        ← 'def' Identifier '(' Identifier ')' EXPRESSION
EXPRESSION        ← TERNARY
TERNARY           ← CONDITION ('?' EXPRESSION ':' EXPRESSION)?
CONDITION         ← INFIX (ConditionOperator INFIX)?
INFIX             ← CALL (InfixOperator CALL)*
CALL              ← PRIMARY ('(' EXPRESSION ')')?
PRIMARY           ← FOR / Identifier / '(' EXPRESSION ')' / Number
FOR               ← 'for' Identifier 'from' Number 'to' Number EXPRESSION

# Token
ConditionOperator ← '<'
InfixOperator     ← '+' / '-'
Identifier        ← !Keyword < [a-zA-Z][a-zA-Z0-9_]* >
Number            ← < [0-9]+ >
Keyword           ← 'def' / 'for' / 'from' / 'to'

%whitespace       ← [ \t\r\n]*
%word             ← [a-zA-Z]
```

License
-------

[MIT](https://github.com/yhirose/fiblang/blob/master/LICENSE)
