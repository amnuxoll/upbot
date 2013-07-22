/**
 * acceptor.c
 *
 *
 * Provides location transparency to service-level components. As
 * noted by Schmidt in "Applying Design Patterns to Flexibly Configure
 * Network Services, acceptors "initialize endpoints of communication
 * at a particular address and wait passively for the other endpoints
 * to connect with it."  That said, the acceptor also allows for the
 * flexibility for application-level to passively wait for services to
 * initiate the connection.
 * 
 * Once a connection is established, neither the application nor the
 * service utilize the acceptor until another connection must be
 * established.
 *
 * @author Tanya L. Crenshaw
 * @since July 2013
 *
 */

#include <stdlib.h>
#include <stddef.h>
#include "acceptor.h"
#include "services.h"

/**
 * accSigchldHandler
 *
 * This function is an interrupt-driven function which 
 * reaps dead children that are forked by the server.
 *
 * This function is local to acceptor.c and should not be made
 * available to other source.
 */
void accSigchldHandler(int s)
{
  // The waitpid() call waits for a child process to reach a certain
  // state.  The WNOHANG option means that the waitpid() call will
  // return immediately if no child has exited.
  while(waitpid(-1, NULL, WNOHANG) > 0);
}

/**
 * accGetInAddr
 *
 * Get sockaddr, IPv4 or IPv6.
 * 
 * This function is local to acceptor.c and should not be made
 * available to other source.
 *
 */
void * accGetInAddr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


/**
 * accCreateConnection
 *
 * Create a passive-mode socket, bind it to a particular port number
 * on the calling host's IP address.
 * 
 * Failure Modes: 
 *
 * If either of the parameters are NULL, the functions
 * returns ACC_BAD_PORT or ACC_NULL_SH.
 *
 * If the socket options cannot be set, the function returns
 * ACC_SOCK_OPT_FAILURE to indicate an error.
 * 
 * If the socket cannot be bound to the address, the function returns
 * ACC_SOCK_BIND_FAILURE to indicate an error.
 *
 * If the socket cannot listen(), the function returns
 * ACC_SOCK_LISTEN_FAILURE.
 *
 * If the child reaper cannot be set using setaction(), the function returns
 * ACC_SIGACTION_FAILURE.
 *
 * Adapted from: "Advance Programming in the UNIX Environment."  page
 * 501 as well as "Beej's Guide to Network Programming."
 *
 * For more details on any of the errors that may occur as the result
 * of this call, users may take advantage of perror() as the socket library
 * sets errno subsequent to its calls.
 *
 * @param[in] port the port number to listen to.
 * 
 * @param[in] type of service (see serviceType enum for possible
 * values).
 *
 * @param[out] sh the serviceHandler that will be partially populated
 * by this call; if successful, its ep field is the handler for the
 * endpoint.
 * 
 * @returns an indication of success or failure.
 */
int accCreateConnection(char * port, serviceType type, serviceHandler * sh)
{

  int result = -1;

  // Sanity-check the incoming parameters.
  if(sh == NULL) return ACC_NULL_SH;
  if(port == NULL) return ACC_BAD_PORT;

  // Set the default values for the serviceHandler.
  servHandlerSetDefaults(sh);

  // Create an endpoint of communication.
  if((result = servCreateEndpoint(SERV_TCP_ENDPOINT, port, sh)) != SERV_SUCCESS) return result;

  // Endpoint of communication has been successfully created, so we
  // can set the service type and port fields in the serviceHandler.
  servHandlerSetService(sh, type);
  servHandlerSetPort(sh, port);

  // Populate sh with the IP of this device.  Note that the IP bound
  // to the socket was likely 0.0.0.0 or 127.0.0.0 since NULL was the
  // first parameter in the getaddrinfo() call.  Instead, we want the
  // IP of the ethernet or wireless interface on this machine.  
  servQueryIP(sh);

  // The passive-mode endpoint has successfully been created.  Now it
  // is time to listen and wait for another entity to approach and
  // accept their connection.
  if (listen(sh->eh, ACC_BACKLOG) == -1) return ACC_SOCK_LISTEN_FAILURE;  // listen() call failed.

  return ACC_SUCCESS;
}

/**
 * accAcceptConnection
 *
 * Given a fully-populated serviceHandler, sh, wait for an approach on
 * the endpoint handler (i.e., socket) prescribed by sh.

 * and set up listening passively for the
 * arrival of connection requests.  Since listening passively can be a
 * blocking call (i.e. accept()), it may be worthwhile to use this
 * function in a separate thread.
 *
 * To reiterate: This function blocks until a connection is
 * established!

 *
 * @returns ACC_SOCK_ACCEPT_FAILURE if the accept() call fails.  Otherwise,
 * ACC_SUCCESS to indicate a succesfully established connection.
 * 
 */
int accAcceptConnection(serviceHandler * sh)
{
  char pee[INET6_ADDRSTRLEN];
  struct sockaddr_storage theirAddr; // connector's address information
  int newSock = -1;
  socklen_t size;
  
  // Wait for an approach.  Note that the accept() call blocks until a
  // connection is established.
  while (newSock == -1) 
    {      
      size = sizeof(theirAddr);
      newSock = accept(sh->eh, (struct sockaddr *)&theirAddr, &size);
      if (newSock == -1)
	{
	  return ACC_SOCK_ACCEPT_FAILURE;
	}
    }

  inet_ntop(theirAddr.ss_family, accGetInAddr((struct sockaddr *)&theirAddr), pee, sizeof(pee));

  return ACC_SUCCESS;
}


/**
 * accBroadcastService
 *
 * Broadcast the service on the network; the service's type, IP, and
 * port are described by sh.
 */
int accBroadcastService(serviceHandler * sh)
{

  // TODO: Get the port from the service handler 
  // instead of using a hard-coded one.

  int howManyBroadcasts = 10;
  int result = -1;  
  int s;       

  static char * bc_addr = "255.255.255.255:10005";
  struct sockaddr_in adr_bc;  /* AF_INET */  
  int len_bc;

  static int so_broadcast = 1;  
  char * bcbuf = "Broadcasting excellent services since 2013!";

  len_bc = sizeof adr_bc;  


  // Create a broadcast address.  
  if( mkaddr(&adr_bc, &len_bc, bc_addr, "udp") == -1)
    {      
      return -1;
    }

  // Create a UDP endpoint of communication for broadcasting the
  // service.
  servCreateEndpoint(SERV_UDP_BROADCAST_ENDPOINT, "10005", sh);

  
  while(howManyBroadcasts)
    {
      
      /* 
       * Broadcast the info
       */  
      result = sendto(sh->bh,  
		      bcbuf,  
		      strlen(bcbuf),  
		      0,  
		      (struct sockaddr *)&adr_bc,  
		      len_bc);   
      
      if ( result == -1 )  
	{
	  perror("sendto\n");
	  return -1;
	}
      
      sleep(2);

      howManyBroadcasts--;
    }
  
}


/**
 * accCompleteConnection
 *
 * Based on D. Schmidt's "Acceptor-Connector" design pattern.
 *
 * 1. Use the passive-mode endpoint, endpointHandler, to create a
 * connected endpoint with a peer.
 *
 * 2. Create a service handler to process data requests arriving from
 * the peer.
 * 
 * 3. "Invoke the service handler's activation hook method which
 * allows the service handler to finish initializing itself."
 *
 * NOTE: Similar to "establishConnection()" in serverUtility.c 
 *
 * @param[in/out] sh the serviceHandler partially populated by a call
 * to accCreateConnection() that will be fully populated by this call.
 * Subsequent read and write operations on this connection are
 * parameterized by this handler.
 *
 * @returns an indication of success or failure.
 *
 */
int accCompleteConnection(serviceHandler * sh)
{

  

}

