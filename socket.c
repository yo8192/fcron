/*
 * FCRON - periodic command scheduler 
 *
 *  Copyright 2000-2002 Thibault Godouet <fcron@free.fr>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 *  The GNU General Public License can also be found in the file
 *  `LICENSE' that comes with the fcron source distribution.
 */

 /* $Id: socket.c,v 1.4 2002-08-10 20:41:46 thib Exp $ */

/* This file contains all fcron's code (server) to handle communication with fcrondyn */


#include "fcron.h"
#include "socket.h"

void exe_cmd(struct fcrondyn_cl *client);
void auth_client(struct fcrondyn_cl *client);
int print_fields(int fd, char print_user, char print_details);
int print_line(int fd, struct CL *line, char print_user, char print_details);

fcrondyn_cl *fcrondyn_cl_base; /* list of connected fcrondyn clients */
int fcrondyn_cl_num = 0;       /* number of fcrondyn clients currently connected */    
fd_set read_set;               /* client fds list : cmd waiting ? */
fd_set master_set;             /* master set : needed since select() modify read_set */
int set_max_fd = 0;            /* needed by select() */
int listen_fd = -1;            /* fd which catches incoming connection */
int auth_fail = 0;             /* number of auth failure */
time_t auth_nofail_since = 0;       /* we refuse auth since x due to too many failures */

void
init_socket(void)
    /* do everything needed to get a working listening socket */
{
    struct sockaddr_un addr;
    int len = 0;

    /* used in fcron.c:main_loop():select() */
    FD_ZERO(&read_set);
    FD_ZERO(&master_set);

    if ( (listen_fd = socket(PF_UNIX, SOCK_STREAM, 0)) == -1 ) {
	error_e("Could not create socket : fcrondyn won't work");
	return;
    }

    addr.sun_family = AF_UNIX;
    if ( (len = strlen(fifofile)) > sizeof(addr.sun_path) ) {
	error("Error : fifo file path too long (max is %d)", sizeof(addr.sun_path));
	goto err;
    }
    strncpy(addr.sun_path, fifofile, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) -1 ] = '\0';

    unlink(fifofile);
    if (bind(listen_fd, (struct sockaddr *) &addr,  sizeof(addr.sun_family)+len) != 0) {
	error_e("Cannot bind socket to '%s'", fifofile);
	goto err;
    }

    if ( listen(listen_fd, MAX_CONNECTION) != 0 ) {
	error_e("Cannot set socket in listen mode");
	goto err;
    }

    /* */
    if ( chmod(fifofile, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH) != 0 )
	error_e("Cannot chmod() socket file");
    /* */
	 
    fcntl(listen_fd, F_SETFD, 1);
    /* set listen_fd to O_NONBLOCK : we do not want fcron to be stopped on error, etc */
    if ( fcntl(listen_fd, F_SETFL, fcntl(listen_fd, F_GETFL) | O_NONBLOCK) == -1 ) {
	error_e("Could not set listen_fd attribute O_NONBLOCK : no fcrondyn support");
	goto err;
    }

    /* no error */
    FD_SET(listen_fd, &master_set);
    if ( listen_fd > set_max_fd )
	set_max_fd = listen_fd;
    
    /* copy master in read_fs, because read_fs will be modified by select() */
    read_set = master_set;
    debug("Socket initialized : listen_fd : %d set_max_fd : %d ", listen_fd, set_max_fd);
    return;

  err:
    close_socket();

}

void
auth_client(struct fcrondyn_cl *client)
    /* check client identity */
{
    char *pass_cry = NULL;
    char *pass_sys = NULL;
    char *pass_str = NULL;

#ifdef HAVE_LIBSHADOW
    struct spwd *pass_sp = NULL;
    if ( (pass_sp = getspnam((char *) client->fcl_cmd )) == NULL ) {
	error_e("could not getspnam %s", (char *) client->fcl_cmd);
	send(client->fcl_sock_fd, "0", sizeof("0"), 0);
	return;
    }
    pass_sys = pass_sp->sp_pwdp;
#else
    struct passwd *pass = NULL;
    if ( (pass = getpwnam((char *) client->fcl_cmd )) == NULL ) {
	error_e("could not getpwnam %s", (char *) client->fcl_cmd);
	send(client->fcl_sock_fd, "0", sizeof("0"), 0);
	return;
    }
    pass_sys = pass->pw_passwd;
#endif

    /* */
    debug("auth_client() : socket : %d", client->fcl_sock_fd);
    /* */

    /* we need to limit auth failures : otherwise fcron may be used to "read"
     * shadow password !!! (or to crack it using a test-all-possible-password attack) */
    if (auth_fail > 0 && auth_nofail_since + AUTH_WAIT <= now )
	/* no auth time exceeded : set counter to 0 */
	auth_fail = 0;
    if (auth_fail >= MAX_AUTH_FAIL) {
	error("Too many authentication failures : try to connect later.");
	send(client->fcl_sock_fd, "0", sizeof("0"), 0);	    
	auth_fail = auth_nofail_since = 0;
	return;    
    }

    /* password is stored after user name */
    pass_str = &( (char *)client->fcl_cmd ) [ strlen( (char*)client->fcl_cmd ) + 1 ];
    if ( (pass_cry = crypt(pass_str, pass_sys)) == NULL ) {
	error_e("could not crypt()");
	send(client->fcl_sock_fd, "0", sizeof("0"), 0);
	return;
    }

/*      debug("pass_sp->sp_pwdp : %s", pass_sp->sp_pwdp); */
/*      debug("pass_cry : %s", pass_cry); */
    if (strcmp(pass_cry, pass_sys) == 0) {
	client->fcl_user = strdup2( (char *) client->fcl_cmd );
	send(client->fcl_sock_fd, "1", sizeof("1"), 0);
    }
    else {
	auth_fail++;
	auth_nofail_since = now;
	error("Invalid passwd for %s from socket %d",
	      (char *) client->fcl_cmd, client->fcl_sock_fd);
	send(client->fcl_sock_fd, "0", sizeof("0"), 0);
    }
}



int
print_fields(int fd, char print_user, char print_details)
    /* print a line describing the field types used in print_line() */
{
    char fields[] =  "  USER ID   SCHEDULE         CMD\n";
    char fields_details[] = "  USER ID    R&Q SCHEDULE         CMD\n";
    char *str = NULL;
    int len = 0;

    str = (print_details) ? fields_details : fields;
    len = (print_details) ? sizeof(fields_details) : sizeof(fields);
    if ( ! print_user ) {
	str += 7;
	len -= 7;
    }

    if ( send(fd, str, len, 0) < 0 )
	error_e("error in send()");

    return OK;
}


int
print_line(int fd, struct CL *line, char print_user, char print_details)
    /* print some basic fields of a line, and some more if details == 1 */
{
    char buf[LINE_LEN];
    int len = 0;
    struct tm *ftime;


    ftime = localtime( &(line->cl_nextexe) );
    len = snprintf(buf, sizeof(buf), "%*s%s%-4ld ",
		   (print_user) ? 6 : 0, (print_user) ? line->cl_file->cf_user : "",
		   (print_user) ? " " : "",
		   line->cl_id);
    if (print_details)
	len += snprintf(buf+len, sizeof(buf)-len, "%4d ", line->cl_numexe);
    len += snprintf(buf+len, sizeof(buf)-len, "%02d/%02d/%d %02d:%02d %s\n",
		   (ftime->tm_mon + 1), ftime->tm_mday, (ftime->tm_year + 1900),
		   ftime->tm_hour, ftime->tm_min,
		   line->cl_shell);

    if ( send(fd, buf, len + 1, 0) < 0 )
	error_e("error in send()");

    return OK;
    
}


void
exe_cmd(struct fcrondyn_cl *client)
    /* read command, and call corresponding function */
{
    long int *cmd;
    int fd;
    char not_found_str[] = "No corresponding job found.\n";
    char not_allowed_str[] = "You are not allowed to see/change this line.\n";
    char cmd_unknown_str[] = "Not implemented yet or erroneous command.\n";
    char details = 0;
    char found = 0;
    char is_root = 0;

    /* to make seems clearer (avoid repeating client->fcl_ all the time */
    cmd = client->fcl_cmd;
    fd = client->fcl_sock_fd;

    /* */
    debug("exe_cmd [0,1,2] : %d %d %d", cmd[0], cmd[1], cmd[2]);
    /* */

    switch ( client->fcl_cmd[0] ) {
    case CMD_DETAILS:
	details = 1;
    case CMD_LIST_JOBS:
    {
	struct job *j;
	char all = (client->fcl_cmd[1] == ALL) ? 1 : 0;

	is_root = (strcmp(client->fcl_user, ROOTNAME) == 0) ? 1 : 0;
	if ( all && ! is_root) {
	    warn("User %s tried to list *all* jobs.", client->fcl_user);
	    send(fd, "you are not allowed to list all jobs.\n",
		 sizeof("you are not allowed to list all jobs.\n"), 0);
	    send(fd, END_STR, sizeof(END_STR), 0);
	    return;
	}
	print_fields(fd, details ? 1 : all, details);
	if ( details ) {
	    for ( j = queue_base; j != NULL; j = j->j_next ) {
		if ( client->fcl_cmd[1] == j->j_line->cl_id ) {
		    if (strcmp(client->fcl_user, j->j_line->cl_file->cf_user) == 0
			|| is_root )
			print_line(fd, j->j_line, 1, 1);
		    else
			send(fd, not_allowed_str, sizeof(not_allowed_str), 0);
		    found = 1;
		    break;
		}
	    }
	}
	else
	    for ( j = queue_base; j != NULL; j = j->j_next ) {
		if (all || strcmp(client->fcl_user, j->j_line->cl_file->cf_user) == 0 ) {
		    print_line(fd, j->j_line, all, 0);
		    found = 1;
		}
	    }
	if ( ! found )
	    send(fd, not_found_str, sizeof(not_found_str), 0);
	/* to tell fcrondyn there's no more data to wait */
	send(fd, END_STR, sizeof(END_STR), 0);
    }
    break;
    
    default:
	send(fd, cmd_unknown_str, sizeof(cmd_unknown_str), 0);
	     
	/* to tell fcrondyn there's no more data to wait */
	send(fd, END_STR, sizeof(END_STR), 0);
    }
}


void
check_socket(int num)
    /* check for new connection, command, connection closed */
{
    int fd = -1, avoid_fd = -1, addr_len = sizeof(struct sockaddr_un);
    struct sockaddr_un client_addr;
    long int buf_int[SOCKET_MSG_LEN];
    int read_len = 0;
    struct fcrondyn_cl *client = NULL, *prev_client = NULL;

    if ( num <= 0 )
	/* no socket to check : go directly to the end of that function */
	goto final_settings;

    debug("Checking socket ...");

    if ( FD_ISSET(listen_fd, &read_set) ) {
	debug("got new connection ...");
	if ((fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len)) == -1) {
	    error_e("could not accept new connection : isset(listen_fd = %d) = %d",
		    listen_fd, FD_ISSET(listen_fd, &read_set));
	}
	else {
	    fcntl(fd, F_SETFD, 1);
	    /* set fd to O_NONBLOCK : we do not want fcron to be stopped on error, etc */
	    if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK) == -1) {
		error_e("Could not set fd attribute O_NONBLOCK : connection rejected.");
		close(fd);
	    }
	    else {
		Alloc(client, fcrondyn_cl);
		client->fcl_sock_fd = fd;
		/* means : not authenticated yet : */
		client->fcl_user = NULL;
		client->fcl_cmd = NULL;

		/* include new entry in client list */
		client->fcl_next = fcrondyn_cl_base;
		fcrondyn_cl_base = client;
		/* to avoid trying to read from it in this call */
		avoid_fd = fd;
		
		FD_SET(fd, &master_set);
		if ( fd > set_max_fd )
		    set_max_fd = fd;
		fcrondyn_cl_num += 1;
		
		debug("Added connection fd : %d - %d connections", fd, fcrondyn_cl_num);
	    }
	}
    }

    client = fcrondyn_cl_base;
    while ( client != NULL ) {
	if (! FD_ISSET(client->fcl_sock_fd, &read_set) || client->fcl_sock_fd==avoid_fd){
	    /* nothing to do on this one ... check the next one */
	    prev_client = client;
	    client = client->fcl_next;
	    continue;
	}

	if ( (read_len = recv(client->fcl_sock_fd, buf_int, sizeof(buf_int), 0)) <= 0 ) {
	    if (read_len == 0) {
		/* connection closed by client */
		close(client->fcl_sock_fd);
		FD_CLR(client->fcl_sock_fd, &master_set);
		debug("connection closed : fd : %d", client->fcl_sock_fd);
		if (prev_client == NULL )
		    client = fcrondyn_cl_base = client->fcl_next;
		else {
		    prev_client->fcl_next = client->fcl_next;
		    Flush(client);
		    client = prev_client->fcl_next;
		}
		fcrondyn_cl_num -= 1;
	    }
	    else {
		error_e("error recv() from sock fd %d", client->fcl_sock_fd);
		prev_client = client;
		client = client->fcl_next;
	    }
	}
	else {
	    client->fcl_cmd_len = read_len;
	    client->fcl_cmd = buf_int;
	    if ( client->fcl_user == NULL )
		/* not authenticated yet */
		auth_client(client);
	    else {
		/* we've just read a command ... */
		client->fcl_idle_since = now;
		exe_cmd(client);
	    }
	    prev_client = client;
	    client = client->fcl_next;
	}
    }

  final_settings:
    /* copy master_set in read_set, because read_set is modified by select() */
    read_set = master_set;
}


void
close_socket(void)
    /* close connections, close socket, remove socket file */
{
    struct fcrondyn_cl *client, *client_buf = NULL;

    if ( listen_fd ) {
	close(listen_fd);
	unlink(fifofile);

	client = fcrondyn_cl_base;
	while ( client != NULL ) {
	    close(client->fcl_sock_fd);

	    client_buf = client->fcl_next;
	    Flush(client->fcl_cmd);
	    Flush(client);
	    fcrondyn_cl_num -= 1;
	    client = client_buf;
	}
    }
}
