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

 /* $Id: socket.c,v 1.1 2002-03-02 17:29:43 thib Exp $ */

/* This file contains all fcron's code (server) to handle communication with fcrondyn */


#include "fcron.h"
#include "socket.h"

void exe_cmd(struct fcrondyn_cl *client);
void auth_client(struct fcrondyn_cl *client);

fcrondyn_cl *fcrondyn_cl_base; /* list of connected fcrondyn clients */
int fcrondyn_cl_num = 0;       /* number of fcrondyn clients currently connected */    
fd_set read_set;               /* client fds list : cmd waiting ? */
fd_set master_set;             /* master set : needed since select() modify read_set */
int set_max_fd = 0;            /* needed by select() */
int listen_fd = -1;          /* fd which catches incoming connection */


void
init_socket(void)
    /* do everything needed to get a working listening socket */
{
    struct sockaddr_un addr;
    int len = 0;

    /* used in fcron.c:main_loop():select() */
    FD_ZERO(&read_set);

    if ( (listen_fd = socket(PF_UNIX, SOCK_STREAM, 0)) == -1 ) {
	error_e("Could not create socket : fcrondyn won't work");
	return;
    }

    addr.sun_family = AF_UNIX;
    if ( (len = strlen(fifofile)) > sizeof(addr.sun_path) ) {
	error("Error : fifo file path too long (max is %d)", sizeof(addr.sun_path));
	goto err;
    }
    strncpy(addr.sun_path, fifofile, sizeof(addr.sun_path));

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
	error_e("Cannot fchmod() socket file");
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
	error("Invalid passwd for %s from socket %d",
	      (char *) client->fcl_cmd, client->fcl_sock_fd);
	send(client->fcl_sock_fd, "0", sizeof("0"), 0);
    }
}


void
exe_cmd(struct fcrondyn_cl *client)
    /* read command, and call corresponding function */
{
    char buf[LINE_LEN];
    int len = 0;

    switch ( client->fcl_cmd[0] ) {
    case CMD_LIST_JOBS:
    {
	struct job *j;
	debug("client->fcl_cmd[1,2,3] : %d %d %d", client->fcl_cmd[0], client->fcl_cmd[1], client->fcl_cmd[2]);
	if ( client->fcl_cmd[1] == ALL && strcmp(client->fcl_user, ROOTNAME) != 0) {
	    warn("User %s tried to list *all* jobs.", client->fcl_user);
	    send(client->fcl_sock_fd, "you are not allowed to list all jobs.\n",
		 sizeof("you are not allowed to list all jobs.\n"), 0);
	    send(client->fcl_sock_fd, END_STR, sizeof(END_STR), 0);
	    return;
	}
	send(client->fcl_sock_fd, "Listing jobs :\n", sizeof("Listing jobs :\n"), 0);
	send(client->fcl_sock_fd, "ID\tCMD\t\n", sizeof("ID\tCMD\t\n"), 0);
	for ( j = queue_base; j != NULL; j = j->j_next ) {
	    if ( client->fcl_cmd[1] == ALL ||
		 strcmp(client->fcl_user, j->j_line->cl_file->cf_user) == 0 ) {
		len = snprintf(buf, sizeof(buf), "%ld job: %s\n", j->j_line->cl_id, j->j_line->cl_shell);
		send(client->fcl_sock_fd, buf, len + 1, 0);
	    }
	}
	/* to tell fcrondyn there's no more data to wait */
	send(client->fcl_sock_fd, END_STR, sizeof(END_STR), 0);
    }
    break;
	
    default:
    {
	
	bzero(buf, sizeof(buf));
	len = snprintf(buf, sizeof(buf), "From exe_cmd() : Hello %s !!\n",
		       client->fcl_user);
	debug("From exe_cmd() : Hello %s !!", client->fcl_user);
	send(client->fcl_sock_fd, buf, len + 1, 0);
	len = snprintf(buf, sizeof(buf), "How are you doing ?\n");
	send(client->fcl_sock_fd, buf, len + 1, 0);
	
	/* to tell fcrondyn there's no more data to wait */
	send(client->fcl_sock_fd, END_STR, sizeof(END_STR), 0);
    }
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
	if ( ! FD_ISSET(client->fcl_sock_fd, &read_set) || client->fcl_sock_fd==avoid_fd){
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
		    Flush(client->fcl_cmd);
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
