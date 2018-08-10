#include <performance_utilities.h>

// connection specifications
const char *domain   = SERVER_NAME;
const char *resource = FILE_NAME;
int port = 80;

// measurement metrics
// bandwidth
double bandwidth[2] = {-1,-1}; // in MB/s
float total_bytes[2] = {0,0};
int n[2] = {0,0};
int skips = SKIPS;
// rtt
double rtt = -1; // in s
// measurement rounds
int rounds = ROUNDS;

// Return time in seconds
double timeval_subtract(struct timeval *x, struct timeval *y)
//double timeval_subtract(auto timeStart, auto timeEnd)
{  

  double diff = x->tv_sec - y->tv_sec;
  diff += (x->tv_usec - y->tv_usec)/1000000.0;

  return diff;
}

/* measure bandwidth (with harmonic mean)
    cur_ts - start_ts define the total time interval.
    bytes is the number of bytes read with the last socket.read() call */
double measure_bw(struct timeval *start_ts, struct timeval *cur_ts, float bytes, int option)
{
  total_bytes[option] += bytes;

  // calculate current measurement
  double ts_diff = timeval_subtract(cur_ts,start_ts);
  double cur_bw = (total_bytes[option]/(1024*1024))/ts_diff;

  if(bandwidth[option] < 0)
    bandwidth[option] = cur_bw; // first measurement
  else
    bandwidth[option] = (n[option]+1)/((n[option]/bandwidth[option])+(1/cur_bw)); // harmonic mean

  n[option]++;

  return bandwidth[option];
}

/* measure Round-Trip Latency (with weighed moving average).
    cur_ts - start_ts is the time between request sent and first response socket.read() */
double measure_rtt(struct timeval *start_ts, struct timeval *cur_ts)
{
  double cur_rtt = timeval_subtract(cur_ts,start_ts);

  if(rtt < 0)
    rtt = cur_rtt; // first measurement
  else
    rtt = 0.8*rtt + 0.2*cur_rtt; // weighed moving average

  return rtt;
}

void make_request(int sock, char *buf, int size)
{
  bzero(buf,size);
  sprintf(buf, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", resource, domain);

  if(send(sock, buf, strlen(buf), 0) < 0)
  {
    perror("Error while sending request");
    return;
  }
}

int create_tcp_connection()
{
  // create socket
  int sock;
  if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      perror("Error : Could not create socket\n");
      return -1;
    }

  // find server
  struct hostent *server;
  server = gethostbyname(domain);
  if(server == NULL)
    {
      perror("Could not find server\n");
      return -1;
    }

  // create address
  struct sockaddr_in serveraddr;
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
  serveraddr.sin_port = htons(port);

  // connect to server
  if(connect(sock, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
    {
      perror("Could not connect to server\n");
      return -1;
    }

  return sock;
}

int main(int argc, char *argv[])
{
  int sock = create_tcp_connection();

  if (sock < 0)
    return -1;

  // prepare timestamps
  struct timeval start_ts, n_pack_ts, first_pack_ts, cur_ts;
  start_ts.tv_sec = 0;
  start_ts.tv_usec = 0;
  n_pack_ts.tv_sec = 0;
  n_pack_ts.tv_usec = 0;
  first_pack_ts.tv_sec = 0;
  first_pack_ts.tv_usec = 0;
  cur_ts.tv_sec = 0;
  cur_ts.tv_usec = 0;

  // prepare objects for blocking IO
  fd_set read_fds;
  struct timeval timeout;
  timeout.tv_sec = TIMEOUT_S;
  timeout.tv_usec = TIMEOUT_US;

  // metrics
  int bytes;
  double bw_a = 0; // option 1
  double total_bw_a = 0;
  double bw_b = 0; // option 2
  double total_bw_b = 0;
  double rtl = 0;
  double total_rtl = 0;

  int activity = 0;
  int counter = 0;

  // buffers
  char *buf = (char *)malloc(BUF_SIZE);
  int req_buf_size = 1024*4; // 4KB
  char *req_buf = (char *)malloc(req_buf_size);

  while(rounds-- > 0)
  {
    // make request
    gettimeofday(&start_ts, NULL);
    make_request(sock, req_buf, req_buf_size);

    // refresh byte count for round
    total_bytes[0] = 0;
    total_bytes[1] = 0;

    // flags
    int first_packet = 1;
    int got_nth_packet = 0;

    FD_ZERO(&read_fds);
    FD_SET(sock, &read_fds);
    timeout.tv_sec = TIMEOUT_S;
    timeout.tv_usec = TIMEOUT_US;
    activity = select(FD_SETSIZE, &read_fds, NULL, NULL, &timeout);

    std::cout << "Start reading packets" << std::flush;
    // receive response and measure metrics
    while( activity )
    {
      gettimeofday(&cur_ts, NULL);
      bytes = read(sock, buf, BUF_SIZE);

      if (counter >= 200)
      {
        std::cout << "." << std::flush;
        counter = 0;
      }

      // OPTION 1: we take the time of the n-th read() as the start time - to handle initial bursts and TCP slow-start
      if(skips > 0)
        skips--;
      else
      {
        if(!got_nth_packet)
        {
          gettimeofday(&n_pack_ts,NULL);
          got_nth_packet = 1;
        }
        else
          bw_a = measure_bw(&n_pack_ts,&cur_ts,bytes,0);
      }

      // very first socket.read()
      if(first_packet)
      {
        // RTT
        rtl = measure_rtt(&start_ts,&cur_ts);

        gettimeofday(&first_pack_ts,NULL);
        first_packet = 0;
      }
      else
      {
        // OPTION 2: we take the request sent timestamp as the start time
        bw_b = measure_bw(&first_pack_ts,&cur_ts,bytes,1);
      }

      FD_ZERO(&read_fds);
      FD_SET(sock, &read_fds);
      timeout.tv_sec = TIMEOUT_S;
      timeout.tv_usec = TIMEOUT_US;
      activity = select(FD_SETSIZE, &read_fds, NULL, NULL, &timeout);

      counter++;
    }

    std::cout << "\nRound " << ROUNDS - rounds << ":" << std::endl;
    std::cout << "Total Kbytes read: " << total_bytes[0]/1000 << std::endl;
    std::cout << "Bandwidth after " << SKIPS << "th packet read: " << bw_a << "MB/s" << std::endl;
    std::cout << "Bandwidth: " << bw_b << "MB/s" << std::endl;
    std::cout << "Round-Trip Latency: " << rtl*1000 << "ms" << std::endl;
    std::cout << "******************************" << std::endl;

    total_bw_a += bw_a;
    total_bw_b += bw_b;
    total_rtl  += rtl;
    counter = 0;
  }

  std::cout << "Average Bandwidth after " << SKIPS << "th packet read: " << total_bw_a/ROUNDS << "MB/s" << std::endl;
  std::cout << "Average Bandwidth: " << total_bw_b/ROUNDS << "MB/s" << std::endl;
  std::cout << "Average Round-Trip Latency: " << (total_rtl*1000)/ROUNDS << "ms" << std::endl;
  std::cout << "******************************" << std::endl;

  // free
  free(req_buf);
  free(buf);

  // close socket
  close(sock);

  return 0;
}
