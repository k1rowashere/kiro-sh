%skeleton "lalr1.cc"
%language "c++"
%require "3.8"

%code requires {
    #include <string>
    #include "command.hpp"
    #include <sys/wait.h>

    namespace cmd {
        class Command;
        class Pipeline;
        class CommandList;
        class driver;
    }
}

%{
    #include "driver.hpp"
    using namespace std;
%}

%define api.token.raw
%define api.token.constructor
%define api.token.prefix {TOK_}

%define api.value.type variant
%define parse.assert
%define parse.trace
%define parse.error detailed
%define parse.lac full

%param {cmd::driver& drv}

%token <std::string> WORD
%token <int> REDIRECT_IN        "<"
%token <int> REDIRECT_OUT       ">"
%token <int> REDIRECT_APPEND    ">>"
%token 
    REDIRECT_APPEND_ERR "&>>"
    REDIRECT_OUT_ERR    "&>"
    QUOTE       "\""
    PIPE        "|"
    PIPE_ERR    "|&"
    LPAREN      "("
    RPAREN      ")"
    OR          "||"
    AND         "&&"
    AMPERSAND   "&"
    SEMICOL     ";"
    EOL         "EOL"
    END         0
;


%type <cmd::Command> command
%type <cmd::Pipeline> pipeline
%type <cmd::CommandList> command_list


%left OR AND
%left SEMICOL AMPERSAND

%%

goal: command_list EOL { return $1.execute(); } 
    | END { exit(0); }
    | EOL { return 0;}
    | "^C" { return 0; }
    ;

command:
    WORD {
        $$ = cmd::Command();
        $$.set_cmd($1);
    }
    | command WORD {
        $1.push_arg($2);
        $$ = $1;
    }
    | command REDIRECT_IN WORD {
        int flags = O_RDONLY;
        $1.push_redirect(cmd::Redirect($2, $3, flags));
        $$ = $1;
    }
    | command REDIRECT_OUT WORD {
        int flags = O_WRONLY | O_CREAT;
        $1.push_redirect(cmd::Redirect($2, $3, flags));
        $$ = $1;
    }
    | command REDIRECT_APPEND WORD {
        int flags =  O_WRONLY | O_APPEND | O_CREAT;
        $1.push_redirect(cmd::Redirect($2, $3, flags));
        $$ = $1;
    }
    | command REDIRECT_OUT_ERR WORD {
        int flags = O_WRONLY | O_CREAT;
        $1.push_redirect(cmd::Redirect(STDOUT_FILENO, $3, flags));
        $1.push_redirect(cmd::Redirect(STDERR_FILENO, $3, flags));
        $$ = $1;    
    }
    | command REDIRECT_APPEND_ERR WORD {
        int flags = O_WRONLY | O_APPEND | O_CREAT;
        $1.push_redirect(cmd::Redirect(STDOUT_FILENO, $3, flags));
        $1.push_redirect(cmd::Redirect(STDERR_FILENO, $3, flags));
        $$ = $1;
    }
    ;

pipeline:
    command {
        $$ = cmd::Pipeline();
        $$.push_back($1);
    }
    | pipeline PIPE command {
        $1.push_back($3);
        $$ = $1;
    }
    | pipeline PIPE_ERR command {
        $3.push_redirect(cmd::Redirect(STDERR_FILENO, STDOUT_FILENO));
        $1.push_back($3);
        $$ = $1;
    }
    ;

command_list:
    pipeline {
        $$ = cmd::CommandList($1);
    }
    | command_list SEMICOL pipeline {
        $1.push_back($3, cmd::JoinMode::THEN);
        $$ = $1;
    }
    | command_list AMPERSAND pipeline {
        $3.set_async();
        $1.push_back($3, cmd::JoinMode::THEN);
        $$ = $1;
    }
    | command_list OR pipeline {
        $1.push_back($3, cmd::JoinMode::OR);
        $$ = $1;
    }
    | command_list AND pipeline {
        $1.push_back($3, cmd::JoinMode::AND);
        $$ = $1;
    }
    | command_list SEMICOL      { $$ = $1; }
    | command_list AMPERSAND    { $1.set_last_async(); $$ = $1; }
    ;

%%
    
// Error handling
void yy::parser::error(const std::string& m) {
    std::cerr << "Error: " << m << std::endl;
}
