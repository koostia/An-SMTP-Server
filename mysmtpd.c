#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024

typedef enum state {
    Undefined,
    // TODO: Add additional states as necessary
    MAIL,
    RCPT,
    DATA
} State;

typedef struct smtp_state {
    int fd;
    net_buffer_t nb;
    char recvbuf[MAX_LINE_LENGTH + 1];
    char *words[MAX_LINE_LENGTH];
    int nwords;
    State state;
    struct utsname my_uname;
    // TODO: Add additional fields as necessary
    user_list_t uList;
} smtp_state;
    
static void handle_client(int fd);

int main(int argc, char *argv[]) {
  
    if (argc != 2) {
	fprintf(stderr, "Invalid arguments. Expected: %s <port>\n", argv[0]);
	return 1;
    }
  
    run_server(argv[1], handle_client);
  
    return 0;
}

// syntax_error returns
//   -1 if the server should exit
//    1  otherwise
int syntax_error(smtp_state *ms) {
    if (send_formatted(ms->fd, "501 %s\r\n", "Syntax error in parameters or arguments") <= 0) return -1;
    return 1;
}

// checkstate returns
//   -1 if the server should exit
//    0 if the server is in the appropriate state
//    1 if the server is not in the appropriate state
int checkstate(smtp_state *ms, State s) {
    if (ms->state != s) {
	if (send_formatted(ms->fd, "503 %s\r\n", "Bad sequence of commands") <= 0) return -1;
	return 1;
    }
    return 0;
}

// All the functions that implement a single command return
//   -1 if the server should exit
//    0 if the command was successful
//    1 if the command was unsuccessful

int do_quit(smtp_state *ms) {
    dlog("Executing quit\n");
    // TODO: Implement this function
    if (send_formatted(ms->fd, "221 Service closing transmission channel\r\n") >= 0) return -1;
    return 1;
}

int do_helo(smtp_state *ms) {
    dlog("Executing helo\n");
    // TODO: Implement this function
    if (send_formatted(ms->fd, "250 %s\r\n", ms->my_uname.nodename) <= 0) return -1;
    ms->state = MAIL;
    return 0;
}

int do_rset(smtp_state *ms) {
    dlog("Executing rset\n");
    // TODO: Implement this function
    ms->state = MAIL;
    return 0;
}

int do_mail(smtp_state *ms) {
    dlog("Executing mail\n");
    // TODO: Implement this function

    int curState = checkstate(ms, MAIL);

    if (curState != 0) {

        return curState;

    }

    // char *start, *end;

    // start = strchr(ms->words[1], '<');
    // end = strchr(ms->words[1], '>');

    // if (start == NULL || end == NULL) {
    //     return syntax_error(ms);
    // }

    ms->uList = user_list_create();

    send_formatted(ms->fd, "250 Requested mail action ok, completed\r\n");

    ms->state = RCPT;

    return 0;
}     

int do_rcpt(smtp_state *ms) {
    dlog("Executing rcpt\n");
    // TODO: Implement this function

    if (ms->state != RCPT || ms->state != DATA) {

        int curState = checkstate(ms, RCPT);

        if (curState != 0) {

        return curState;

        }

    }

    char *oldAddress = malloc(MAX_USERNAME_SIZE);
    *oldAddress = ms->words[1];

    char *start = strchr(oldAddress, '<');
    start++;
    char *end = strchr(oldAddress, '>');

    if (start == NULL) {
        return syntax_error(ms);
    } else if (end == NULL) {
        return syntax_error(ms);
    }

    char *email = malloc(MAX_USERNAME_SIZE);
    strncpy(email, start, end - start);

    if (is_valid_user(email, NULL) == 0) {

        send_formatted(ms->fd, "550 No such user - %s\r\n", email);
        return 1;

    } else {

        user_list_add(&ms->uList, email);
        send_formatted(ms->fd, "250 Requested mail action ok, completed\r\n");
        ms->state = DATA;

    }

    return 0;

    // dlog("Executing rcpt\n");
    // // TODO: Implement this function

    // int curState = checkstate(ms, RCPT);

    // if (curState != 0) {

    //     return curState;

    // }

    // char *oldAddress = ms->words[1];
    // char *start = strchr(oldAddress, '<');
    // if (start == NULL) {
    //     return syntax_error(ms);
    // }
    // char *end = strchr(oldAddress, '>');
    // if (end == NULL) {
    //     return syntax_error(ms);
    // }
    // size_t email_len = end - start - 1;
    // char *email = malloc(email_len + 1);
    // if (email == NULL) {
    //     return syntax_error(ms);
    // }
    // strncpy(email, start + 1, email_len);
    // email[email_len] = '\0';

    // if (is_valid_user(email, NULL) == 0) {

    //     send_formatted(ms->fd, "550 No such user - %s\r\n", email);
    //     return 1;

    // } else {

    //     user_list_add(&ms->uList, email);
    //     send_formatted(ms->fd, "250 Requested mail action ok, completed\r\n");
    //     ms->state = DATA;

    // }

    // return 0;
}     

int do_data(smtp_state *ms) {
    dlog("Executing data\n");
    // TODO: Implement this function

    send_formatted(ms->fd, "354 Waiting for data, finish with <CR><LF>.<CR><LF>\r\n");

    int curState = checkstate(ms, DATA);

    if (curState != 0) {

        return curState;

    }

    

    return 0;
}     
      
int do_noop(smtp_state *ms) {
    dlog("Executing noop\n");
    // TODO: Implement this function
    if (send_formatted(ms->fd, "250 OK (noop)\r\n") <= 0) return -1;
    return 0;
}

int do_vrfy(smtp_state *ms) {
    dlog("Executing vrfy\n");
    // TODO: Implement this function

    // char *start, *end;

    // start = strchr(ms->words[1], '<');
    // end = strchr(ms->words[1], '>');

    // if (start == NULL || end == NULL) {
    //     return syntax_error(ms);
    // }

    if(is_valid_user(ms->words[1], NULL) != 0) {
        if (send_formatted(ms->fd, "250 - %s\r\n", ms->words[1]) <= 0) return -1;
    } else {
        if (send_formatted(ms->fd, "550 No such user - %s\r\n", ms->words[1]) <= 0) return -1;
    }


    return 0;
}

void handle_client(int fd) {
  
    size_t len;
    smtp_state mstate, *ms = &mstate;
  
    ms->fd = fd;
    ms->nb = nb_create(fd, MAX_LINE_LENGTH);
    ms->state = Undefined;
    uname(&ms->my_uname);
    
    if (send_formatted(fd, "220 %s Service ready\r\n", ms->my_uname.nodename) <= 0) return;

  
    while ((len = nb_read_line(ms->nb, ms->recvbuf)) >= 0) {
	if (ms->recvbuf[len - 1] != '\n') {
	    // command line is too long, stop immediately
	    send_formatted(fd, "500 Syntax error, command unrecognized\r\n");
	    break;
	}
	if (strlen(ms->recvbuf) < len) {
	    // received null byte somewhere in the string, stop immediately.
	    send_formatted(fd, "500 Syntax error, command unrecognized\r\n");
	    break;
	}
    
	// Remove CR, LF and other space characters from end of buffer
	while (isspace(ms->recvbuf[len - 1])) ms->recvbuf[--len] = 0;
    
	dlog("Command is %s\n", ms->recvbuf);
    
	// Split the command into its component "words"
	ms->nwords = split(ms->recvbuf, ms->words);
	char *command = ms->words[0];
    
	if (!strcasecmp(command, "QUIT")) {
	    if (do_quit(ms) == -1) break;
	} else if (!strcasecmp(command, "HELO") || !strcasecmp(command, "EHLO")) {
	    if (do_helo(ms) == -1) break;
	} else if (!strcasecmp(command, "MAIL")) {
	    if (do_mail(ms) == -1) break;
	} else if (!strcasecmp(command, "RCPT")) {
	    if (do_rcpt(ms) == -1) break;
	} else if (!strcasecmp(command, "DATA")) {
	    if (do_data(ms) == -1) break;
	} else if (!strcasecmp(command, "RSET")) {
	    if (do_rset(ms) == -1) break;
	} else if (!strcasecmp(command, "NOOP")) {
	    if (do_noop(ms) == -1) break;
	} else if (!strcasecmp(command, "VRFY")) {
	    if (do_vrfy(ms) == -1) break;
	} else if (!strcasecmp(command, "EXPN") ||
		   !strcasecmp(command, "HELP")) {
	    dlog("Command not implemented \"%s\"\n", command);
	    if (send_formatted(fd, "502 Command not implemented\r\n") <= 0) break;
	} else {
	    // invalid command
	    dlog("Illegal command \"%s\"\n", command);
	    if (send_formatted(fd, "500 Syntax error, command unrecognized\r\n") <= 0) break;
	}
    }
  
    nb_destroy(ms->nb);
}
