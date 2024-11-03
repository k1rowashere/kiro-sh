%skeleton "lalr1.cc"
%language "c++"
%require "3.8"

%code requires {
    #include <string>
    #include "command.hpp"
    #include <sys/wait.h>

    namespace cmd {
        class SimpleCommand;
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
%printer { yyo << $$; } <*>;

%token <std::string> WORD
%token <int> REDIRECT_IN        "<"
%token <int> REDIRECT_OUT       ">"
%token <int> REDIRECT_APPEND    ">>"
%token 
    REDIRECT_APPEND_ERR "&>>"
    REDIRECT_OUT_ERR    "&>"
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


%type <cmd::SimpleCommand> simple_command
%type <cmd::Pipeline> pipeline
%type <cmd::CommandList> command_list


%left OR AND
%left SEMICOL AMPERSAND

%%

goal: command_list EOL { return $1.execute(); } 
    | END { exit(0); }
    ;

simple_command:
    WORD {
        // cout << "Command: " << $1 << endl;
        $$ = cmd::SimpleCommand();
        $$.set_cmd($1);
    }
    | simple_command WORD {
        // cout << "+ arg: " << $2 << endl;
        $1.push_arg($2);
        $$ = $1;
    }
    | simple_command REDIRECT_IN WORD {
        // cout << "+ redirect in: " << $3 << endl;
        int flags = O_RDONLY;
        $1.push_redirect(cmd::Redirect($2, $3, flags));
        $$ = $1;
    }
    | simple_command REDIRECT_OUT WORD {
        // cout << "+ redirect out: " << $3 << endl;
        int flags = O_WRONLY | O_CREAT;
        $1.push_redirect(cmd::Redirect($2, $3, flags));
        $$ = $1;
    }
    | simple_command REDIRECT_APPEND WORD {
        // cout << "+ redirect append: " << $3 << endl;
        int flags =  O_WRONLY | O_APPEND | O_CREAT;
        $1.push_redirect(cmd::Redirect($2, $3, flags));
        $$ = $1;
    }
    | simple_command REDIRECT_OUT_ERR WORD {
        // cout << "+ redirect out err: " << $3 << endl;
        int flags = O_WRONLY | O_CREAT;
        $1.push_redirect(cmd::Redirect(STDOUT_FILENO, $3, flags));
        $1.push_redirect(cmd::Redirect(STDERR_FILENO, $3, flags));
        $$ = $1;    
    }
    | simple_command REDIRECT_APPEND_ERR WORD {
        // cout << "+ redirect append err: " << $3 << endl;
        int flags = O_WRONLY | O_APPEND | O_CREAT;
        $1.push_redirect(cmd::Redirect(STDOUT_FILENO, $3, flags));
        $1.push_redirect(cmd::Redirect(STDERR_FILENO, $3, flags));
        $$ = $1;
    }
    ;

pipeline:
    simple_command {
        // cout << "Pipeline: " << $1 << endl;
        $$ = cmd::Pipeline();
        $$.push_back($1);
    }
    | pipeline PIPE simple_command {
        // cout << "+ cmd to pipe: " << $1 << " | " << $3 << endl;
        $1.push_back($3);
        $$ = $1;
    }
    | pipeline PIPE_ERR simple_command {
        // cout << "+ cmd to pipe err: " << $1 << " |& " << $3 << endl;
        $3.push_redirect(cmd::Redirect(STDERR_FILENO, STDOUT_FILENO));
        $1.push_back($3);
        $$ = $1;
    }
    ;

command_list:
    pipeline {
        // cout << "CommandList: " << $1 << endl;
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
