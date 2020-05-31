
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

int player_count = 0;
pthread_mutex_t mutexcount;

void error(const char *msg)
{
    perror(msg);
    pthread_exit(NULL);
}


/* Reads an int from a client socket. */
int recv_int(int cli_sockfd)
{
    int msg = 0;
    int n = read(cli_sockfd, &msg, sizeof(int));
    
    if (n < 0 || n != sizeof(int)) /* Not what we were expecting. Client likely disconnected. */
        return -1;
    
    return msg;
}

/*
 * Socket Write Functions
 */

/* Writes a message to a client socket. */
void write_client_msg(int cli_sockfd, char * msg)
{
    int n = write(cli_sockfd, msg, strlen(msg));
    if (n < 0)
        error("ERROR writing msg to client socket");
}

/* Writes an int to a client socket. */
void write_client_int(int cli_sockfd, int msg)
{
    int n = write(cli_sockfd, &msg, sizeof(int));
    if (n < 0)
        error("ERROR writing int to client socket");
}

/* Writes a message to both client sockets. */
void write_clients_msg(int * cli_sockfd, char * msg)
{
    write_client_msg(cli_sockfd[0], msg);
    write_client_msg(cli_sockfd[1], msg);
}

/* Writes an int to both client sockets. */
void write_clients_int(int * cli_sockfd, int msg)
{
    write_client_int(cli_sockfd[0], msg);
    write_client_int(cli_sockfd[1], msg);
}

/*
 * Connect Functions
 */

/* Sets up the listener socket. */
int setup_listener(int portno)
{
    int sockfd;
    struct sockaddr_in serv_addr;

    /* Get a socket to listen on */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening listener socket.");
    
    /* Zero out the memory for the server information */
    memset(&serv_addr, 0, sizeof(serv_addr));
    
    /* set up the server info */
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_addr.s_addr = INADDR_ANY; 
    serv_addr.sin_port = htons(portno);     

    /* Bind the server info to the listener socket. */
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR binding listener socket.");

    /* Return the socket number. */
    return sockfd;
}

/* Sets up the client sockets and client connections. */
void get_clients(int lis_sockfd, int * cli_sockfd)
{
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    /* Listen for two clients. */
    int num_conn = 0;
    while(num_conn < 2)
    {
        /* Listen for clients. */
        listen(lis_sockfd, 5);
        
        /* Zero out memory for the client information. */
        memset(&cli_addr, 0, sizeof(cli_addr));

        clilen = sizeof(cli_addr);
    
        /* Accept the connection from the client. */
        cli_sockfd[num_conn] = accept(lis_sockfd, (struct sockaddr *) &cli_addr, &clilen);
    
        if (cli_sockfd[num_conn] < 0)
            /* Horrible things have happened. */
            error("ERROR accepting a connection from a client.");
        
        /* Send the client it's ID. */
        write(cli_sockfd[num_conn], &num_conn, sizeof(int));
        
        
        /* Increment the player count. */
       // pthread_mutex_lock(&mutexcount);
        player_count++;
        printf("Number of players is now %d.\n", player_count);
      //  pthread_mutex_unlock(&mutexcount);

        if (num_conn == 0) {
            /* Send "HLD" to first client to let the user know the server is waiting on a second client. */
            write_client_msg(cli_sockfd[0],"HLD");
            
          
        }

        num_conn++;
    }
}

/*
 * Game Functions
 */

/* Gets a move from a client. */
int get_player_move(int cli_sockfd)
{
    
    /* Tell player to make a move. */
    write_client_msg(cli_sockfd, "TRN");

    /* Get players move. */
    return recv_int(cli_sockfd);
}

/* Checks that a players move is valid. */
int check_move(char board[][3], int move, int player_id)
{
    if ((move == 9) || (board[move/3][move%3] == ' ')) { /* Move is valid. */
             
        return 1;
   }
   else { /* Move is invalid. */
       return 0;
   }
}

/* Updates the board with a new move. */
void update_board(char board[][3], int move, int player_id)
{
    board[move/3][move%3] = player_id ? 'X' : 'O';
}

/* Draws the game board to stdout. */
void draw_board(char board[][3])
{
    printf(" %c | %c | %c \n", board[0][0], board[0][1], board[0][2]);
    printf("-----------\n");
    printf(" %c | %c | %c \n", board[1][0], board[1][1], board[1][2]);
    printf("-----------\n");
    printf(" %c | %c | %c \n", board[2][0], board[2][1], board[2][2]);
}

/* Sends a board update to both clients. */
void send_update(int * cli_sockfd, int move, int player_id)
{

    /* Signal an update */    
    write_clients_msg(cli_sockfd, "UPD");

    /* Send the id of the player that made the move. */
    write_clients_int(cli_sockfd, player_id);
    
    /* Send the move. */
    write_clients_int(cli_sockfd, move);

}

/* Sends the number of active players to a client. */
void send_player_count(int cli_sockfd)
{
    write_client_msg(cli_sockfd, "CNT");
    write_client_int(cli_sockfd, player_count);
}

/* Checks the board to determine if there is a winner. */
int check_board(char board[][3], int last_move)
{

    int row = last_move/3;
    int col = last_move%3;

    if ( board[row][0] == board[row][1] && board[row][1] == board[row][2] ) { /* Check the row for a win. */

        return 1;
    }
    else if ( board[0][col] == board[1][col] && board[1][col] == board[2][col] ) { /* Check the column for a win. */        
        return 1;
    }
    else if (!(last_move % 2)) { /* If the last move was at an even numbered position we have to check the diagonal(s) as well. */
        if ( (last_move == 0 || last_move == 4 || last_move == 8) && (board[1][1] == board[0][0] && board[1][1] == board[2][2]) ) {  /* Check backslash diagonal. */

            return 1;
        }
        if ( (last_move == 2 || last_move == 4 || last_move == 6) && (board[1][1] == board[0][2] && board[1][1] == board[2][0]) ) { /* Check frontslash diagonal. */

            return 1;
        }
    }

    /* No winner, yet. */
    return 0;
}

/* Runs a game between two clients. */
void *run_game(void *thread_data) 
{
    int *cli_sockfd = (int*)thread_data; /* Client sockets. */
    char board[3][3] = { {' ', ' ', ' '}, /* Game Board */ 
                         {' ', ' ', ' '}, 
                         {' ', ' ', ' '} };

    printf("Game on!\n");
    
    /* Send the start message. */
    write_clients_msg(cli_sockfd, "SRT");

    draw_board(board);
    
    int prev_player_turn = 1;
    int player_turn = 0;
    int game_over = 0;
    int turn_count = 0;
    while(!game_over) {
        /* Tell other player to wait, if necessary. */
        if (prev_player_turn != player_turn)
            write_client_msg(cli_sockfd[(player_turn + 1) % 2], "WAT");

        int valid = 0;
        int move = 0;
        while(!valid) { /* We need to keep asking for a move until the player's move is valid. */
            move = get_player_move(cli_sockfd[player_turn]);
            if (move == -1) break; /* Error reading client socket. */

            printf("Player %d played position %d\n", player_turn, move);
                
            valid = check_move(board, move, player_turn);
            if (!valid) { /* Move was invalid. */
                printf("Move was invalid. Let's try this again...\n");
                write_client_msg(cli_sockfd[player_turn], "INV");
            }
        }

        if (move == -1) { /* Error reading from client. */
            printf("Player disconnected.\n");
            break;
        }
        else if (move == 9) { /* Send the client the number of active players. */
            prev_player_turn = player_turn;
            send_player_count(cli_sockfd[player_turn]);
        }
        else {
            /* Update the board and send the update. */
            update_board(board, move, player_turn);
            send_update( cli_sockfd, move, player_turn );
                
            /* Re-draw the board. */
            draw_board(board);

            /* Check for a winner/loser. */
            game_over = check_board(board, move);
            
            if (game_over == 1) { /* We have a winner. */
                write_client_msg(cli_sockfd[player_turn], "WIN");
                write_client_msg(cli_sockfd[(player_turn + 1) % 2], "LSE");
                printf("Player %d won.\n", player_turn);
            }
            else if (turn_count == 8) { /* There have been nine valid moves and no winner, game is a draw. */
                printf("Draw.\n");
                write_clients_msg(cli_sockfd, "DRW");
                game_over = 1;
            }

            /* Move to next player. */
            prev_player_turn = player_turn;
            player_turn = (player_turn + 1) % 2;
            turn_count++;
        }
    }

    printf("Game over.\n");

    /* Close client sockets and decrement player counter. */
    close(cli_sockfd[0]);
    close(cli_sockfd[1]);
    
    free(cli_sockfd);

    pthread_exit(NULL);
}

/* 
 * Main Program
 */

int main(int argc, char *argv[])
{   
    /* Make sure a port was specified. */
    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }
    
    int lis_sockfd = setup_listener(atoi(argv[1])); /* Listener socket. */
    pthread_mutex_init(&mutexcount, NULL);

    while (1) {
        if (player_count <= 5) { /* Only launch a new game if we have room. Otherwise, just spin. */  
            int *cli_sockfd = (int*)malloc(2*sizeof(int)); /* Client sockets */
            memset(cli_sockfd, 0, 2*sizeof(int));
            
            /* Get two clients connected. */
            get_clients(lis_sockfd, cli_sockfd);

            pthread_t thread; /* Don't really need the thread id for anything in this case, but here it is anyway. */
            int result = pthread_create(&thread, NULL, run_game, (void *)cli_sockfd); /* Start a new thread for this game. */
            if (result){
                printf("Thread creation failed with return code %d\n", result);
                exit(-1);
            }
    }

    close(lis_sockfd);

    pthread_mutex_destroy(&mutexcount);
    pthread_exit(NULL); 
}
}
