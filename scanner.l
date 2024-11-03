%{
    #include <string>
    #include <regex>
    #include "parser.hpp"
    #include "driver.hpp"
    #define yyterminate() return yy::parser::make_END();
%}

%option noyywrap noinput nounput
/*%option debug*/

ws      [ \t]
nws     [^ \t\n]
sep     [ \t\n\(\)\{\}\|&;<>] 
nsep    [^ \t\n\(\)\{\}\|&;<>] 
/* Matches Identifiers (variables) */

%%

    /*
     * Redirection 
     */

\d*>    {
    int dest = yyleng >= 2 ? std::stoi(yytext) : STDOUT_FILENO;
    return yy::parser::make_REDIRECT_OUT(dest);
}

\d*<    {
    int dest = yyleng >= 2 ? std::stoi(yytext) : STDIN_FILENO;
    return yy::parser::make_REDIRECT_IN(dest);
}

\d*>>   {
    int dest = yyleng >= 3 ? std::stoi(yytext) : STDOUT_FILENO;
    return yy::parser::make_REDIRECT_APPEND(dest); 
}

"&>>"   return yy::parser::make_REDIRECT_APPEND_ERR(); 

    /*
     * Control Tokens
     */

"|"     return yy::parser::make_PIPE(); 
"|&"    return yy::parser::make_PIPE_ERR(); 

"("     return yy::parser::make_LPAREN(); 
")"     return yy::parser::make_RPAREN(); 
"||"    return yy::parser::make_OR(); 
"&&"    return yy::parser::make_AND(); 
"&"     return yy::parser::make_AMPERSAND(); 
";"     return yy::parser::make_SEMICOL(); 
<<EOF>> return yy::parser::make_END(); 
\n      return yy::parser::make_EOL(); 

    /*
     * single quote (literal string w/o escaping) 
     */

'.*'    {
    auto str = std::string(yytext);
    return yy::parser::make_WORD(str.substr(1, str.size() - 2));
}

    /*
     * TODO: double quote
     */

({nsep}|\\{sep})+  {
    //  TODO: handle keywords

    // remove backslashes except when escaped
    auto str = std::regex_replace(yytext, std::regex("\\\\(.)"), "$1");

    return yy::parser::make_WORD(str);
}

{ws}+   ;
%%
