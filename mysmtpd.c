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
    // If send_formatted was successfully send, return -1 that the server should exit.
    if (send_formatted(ms->fd, "221 Service closing transmission channel\r\n") >= 0) return -1;
    return 1;
}

int do_helo(smtp_state *ms) {
    dlog("Executing helo\n");
    // TODO: Implement this function
    if (send_formatted(ms->fd, "250 %s\r\n", ms->my_uname.nodename) <= 0) return -1;
    // Set the state to MAIL
    ms->state = MAIL;
    return 0;
}

int do_rset(smtp_state *ms) {
    dlog("Executing rset\n");
    // TODO: Implement this function

    if (ms->state == MAIL) {

        // If the state is already MAIL, no need to call user_list_destroy
        if (send_formatted(ms->fd, "250 State reset\r\n") <= 0) return -1;
        return 0;

    }

    // If state was any other state other than mail, reset state table, then call user_list_destroy
    ms->state = MAIL;
    user_list_destroy(ms->uList);
    if (send_formatted(ms->fd, "250 State reset\r\n") <= 0) return -1;
    return 0;
}

int do_mail(smtp_state *ms) {
    dlog("Executing mail\n");
    // TODO: Implement this function

    // Check if it's in the right state
    int curState = checkstate(ms, MAIL);
    char substring[] = "FROM:";
    int subLen;
    subLen = strlen(substring);

    if (curState != 0) {
        // If wrong state, return 1;
        return curState;
    }

    if (ms->words[1] == NULL) {
        // If there are no words following MAIL command, return syntax_error
        return syntax_error(ms);
    }

    char *start, *end;
    start = strchr(ms->words[1], '<');
    end = strchr(ms->words[1], '>');

    if (start == NULL || end == NULL) {
        // If both angle brackets don't exist, return syntax_error
        int syntax = syntax_error(ms);
        return syntax;
    }

    if (strncmp(ms->words[1], substring, subLen) == 0) {
        // If the words contains "FROM:" at the beginning, create user list, and change state.
        // Else return syntax_error
        ms->uList = user_list_create();
        send_formatted(ms->fd, "250 Requested mail action ok, completed\r\n");
        ms->state = RCPT;
        return 0;
    } else {
        return syntax_error(ms);
    }

    return 0;
    
}     

int do_rcpt(smtp_state *ms) {
    dlog("Executing rcpt\n");
    // TODO: Implement this function

    if (ms->state != RCPT && ms->state != DATA) {
        // If the state is not RCPT and DATA, check if the state is just RCPT 
        int curState = checkstate(ms, RCPT);
        if (curState != 0) {
            // If wrong state, return checkstate error
            return curState;
        }
    }

    if (ms->words[1] == NULL) {
        // If there are no words following RCPT command, return syntax_error
        return syntax_error(ms);
    }

    char substring[] = "TO:";
    int subLen;
    subLen = strlen(substring);

    if (strncmp(ms->words[1], substring, subLen) != 0) {
        // If the word is missing "TO:" at the beginning, return syntax_error
        return syntax_error(ms);
    }

    char *start, *end;
    start = strchr(ms->words[1], '<');
    end = strchr(ms->words[1], '>');

    if (start == NULL || end == NULL) {
        // If both angle brackets don't exist, return syntax_error
        return syntax_error(ms);
    }

    // Copy email address without the angle brackets
    start++;
    char *email = malloc(MAX_USERNAME_SIZE);
    strncpy(email, start, end - start);

    if (is_valid_user(email, NULL) == 0) {
        // If valid user does not exist, return 1
        send_formatted(ms->fd, "550 No such user - %s\r\n", email);
        return 1;
    } else {
        // If valid user does exist, add email to the user_list, and change state to DATA
        user_list_add(&ms->uList, email);
        send_formatted(ms->fd, "250 Requested mail action ok, completed\r\n");
        ms->state = DATA;
    }

    return 0;
}     

int do_data(smtp_state *ms) {
    dlog("Executing data\n");
    // TODO: Implement this function

    int curState = checkstate(ms, DATA);
    if (curState != 0) {
        // Check if current state is DATA, if not return checkstate error
        return curState;
    }

    // Create a temp. file
    char template[MAX_USERNAME_SIZE] = "example.XXXXXX";
    int fdTemplate = mkstemp(template);
    FILE *fpt = fdopen(fdTemplate, "w");

    send_formatted(ms->fd, "354 Waiting for data, finish with <CR><LF>.<CR><LF>\r\n");

    // Keep looping until ".\r\n" is reached, signifying the end.
    while (1) {
        // Read lines from the buffer
        int msg = nb_read_line(ms->nb, ms->recvbuf);
    
        if (msg == 3) {
            // If ".\r\n" is reached, close the file, change state back to MAIL, and break from the loop
            send_formatted(ms->fd, "250 Requested mail action ok, completed\r\n");
            fclose(fpt);
            ms->state = MAIL;
            break;
        }

        if (msg == 2) {
            // If "\r\n" is found, print onto the file
            fwrite("\r\n", 1, 2, fpt);
        } else {

            // Find the pointer to the first period
            char *result;
            char *p = strchr(ms->recvbuf, '.');

            if (p == ms->recvbuf) {
                // If the pointer is equal to the rest of the recieve buffer, this signifys that the
                // period is at the beginning of the line. We skip the period and write the new line
                // without the period at the start in the file.
                result = &ms->recvbuf[1];
                fwrite(result, sizeof(char), msg - 1, fpt);
            } else {
                // If not, write the rest of the recieve buffer onto the file.
                fwrite(ms->recvbuf, sizeof(char), msg, fpt);
            }

        }

    }

    // Save the user mail under all users that we added to the user_list
    save_user_mail(template, ms->uList);
    // Unlink the temp. file
    unlink(template);
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

    char *start, *end;
    if (ms->words[1] == NULL) {
        // If there are no words following VRFY command, return syntax_error
        return syntax_error(ms);
    }

    start = strchr(ms->words[1], '<');
    end = strchr(ms->words[1], '>');
    char *email = malloc(MAX_USERNAME_SIZE);

    if (start == NULL || end == NULL) {
        // If both angle brackets don't exist, check if a word exists
        size_t len = strlen(ms->words[1]);

        if (len == 0) {
            // If there are no words, return syntax_error
            int syntax = syntax_error(ms);
            return syntax;
        } else {
            // Else assign the email as the word
            email = ms->words[1];

        }

    } else {
        // If there are angle brackets, extract the words between them and assign as the email
        start++;
        strncpy(email, start, end - start);
    }

    if(is_valid_user(email, NULL) != 0) {
        // Check if user is valid
        if (send_formatted(ms->fd, "250 - %s\r\n", email) <= 0) return -1;
    } else {
        if (send_formatted(ms->fd, "550 No such user - %s\r\n", email) <= 0) return -1;
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