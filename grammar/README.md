# SLON Grammar

This directory contains the canonical [Peggy](https://peggyjs.org/) (PEG.js-compatible) grammar that defines the Single Line Object Notation syntax.

## Building a Parser

Generate a CommonJS parser with:

```bash
npx peggy --format commonjs --output ../dist/slon-parser.js slon.pegjs
```

Adjust `--output` to your preferred destination. The resulting module exposes a single `parse` function that returns JavaScript values (`Object`, `Array`, primitives, and `Date` instances for datetime literals).

## Grammar Highlights

- `SLON_text` is the entry rule; it trims leading/trailing whitespace before parsing a value.
- Objects use parentheses for delimiters, while arrays retain JSON’s square brackets but swap commas for pipe separators.
- Datetime literals follow `YYYY-MM-DD/HH:MM:SS.mmm` and are transformed into JavaScript `Date` objects in UTC.
- Unquoted strings absorb everything until `:`, `,`, `(`, `)`, or quotes; use single or double quotes when you need punctuation or whitespace preserved.
- All JSON escape sequences are supported, including Unicode (`\uXXXX`).

## Developing the Grammar

- Install Peggy locally for iterative work: `npm install --save-dev peggy`.
- Regenerate the parser after changes: `npx peggy --watch ...` can rebuild on save.
- Add your own unit tests by requiring the generated parser and asserting on the resulting JavaScript values.

Keeping the grammar small and explicit makes it easy to embed SLON support in environments where full JSON would be visually noisy.

## EBNF

```
slon        = ws , value , ws ;
value       = "true"
            | "false"
            | "null"
            | datetime
            | object
            | array
            | number
            | string ;

object      = "(" , [ member , { "," , member } ] , ")" ;
member      = string , ":" , value ;

array       = "[" , [ value , { "|" , value } ] , "]" ;

datetime    = digit4 , "-" , digit2 , "-" , digit2 ,
              "/" , digit2 , ":" , digit2 , ":" , digit2 , "." , digit3 ;

number      = [ "-" ] , integer , [ fraction ] , [ exponent ] ;
integer     = "0" | ( digit1-9 , { digit } ) ;
fraction    = "." , digit , { digit } ;
exponent    = ( "e" | "E" ) , [ "+" | "-" ] , digit , { digit } ;

string      = quoted_string | bare_string ;
quoted_string = ( "'" , { char | escape } , "'" )
              | ( "\"" , { char | escape } , "\"" ) ;
bare_string = bare_char , { bare_char } ;

escape      = "\" , ( "'" | "\"" | "\" | "/" | "b" | "f" | "n" | "r" | "t"
              | "u" , hex , hex , hex , hex ) ;

ws          = { " " | "\t" | "\n" | "\r" } ;

bare_char   = ? any character except ":", ",", "(", ")", "[", "]", "|", quotes or control chars ? ;
char        = ? any Unicode scalar value except control characters ? ;
digit       = "0" | digit1-9 ;
digit1-9    = "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" ;
digit2      = digit , digit ;
digit3      = digit , digit , digit ;
digit4      = digit , digit , digit , digit ;
hex         = digit | "a" | "b" | "c" | "d" | "e" | "f" | "A" | "B" | "C" | "D" | "E" | "F" ;
```

## ABNF

```
slon        = ws value ws
value       = true / false / null / datetime / object / array / number / string

object      = "(" [ member *( "," member ) ] ")"
member      = string ":" value

array       = "[" [ value *( "|" value ) ] "]"

datetime    = year "-" month "-" day "/" hour ":" minute ":" second "." msecond
year        = 4DIGIT
month       = 2DIGIT
day         = 2DIGIT
hour        = 2DIGIT
minute      = 2DIGIT
second      = 2DIGIT
msecond     = 3DIGIT

number      = [ "-" ] int [ frac ] [ exp ]
int         = "0" / ( DIGIT1-9 *DIGIT )
frac        = "." 1*DIGIT
exp         = ( "e" / "E" ) [ "+" / "-" ] 1*DIGIT

string      = quoted / bare
quoted      = ( DQUOTE *( qchar / escape ) DQUOTE )
            / ( SQUOTE *( qchar / escape ) SQUOTE )
bare        = 1*bare-char

escape      = "\" ( DQUOTE / SQUOTE / "\" / "/" / "b" / "f" / "n" / "r" / "t" /
              ( "u" 4HEXDIG ) )

ws          = *( SP / HTAB / CR / LF )

bare-char   = %x21 / %x23-27 / %x2A-39 / %x3B-5A / %x5C / %x5E-7E / %x80-10FFFF
              ; printable characters except colon, comma, brackets, pipe, quotes

true        = "true"
false       = "false"
null        = "null"

DIGIT       = %x30-39
DIGIT1-9    = %x31-39
DQUOTE      = %x22
SQUOTE      = %x27
HEXDIG      = DIGIT / %x41-46 / %x61-66
qchar       = %x20-21 / %x23-5B / %x5D-10FFFF
```
