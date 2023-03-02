#!/usr/bin/perl

# Tests for `listen ip:port udp kcp` directive.

###############################################################################

use warnings;
use strict;

use Test::More;

BEGIN { use FindBin; chdir($FindBin::Bin); }

use lib 'lib';
use Test::Nginx;
use Test::Nginx::Stream qw/ dgram /;
use Time::HiRes qw/ gettimeofday usleep /;

###############################################################################

STDERR->autoflush(1);
STDOUT->autoflush(1);

my $t = Test::Nginx->new()->has(qw/stream stream_return udp kcp/)->plan(0);

$t->write_file_expand('nginx.conf', <<'EOF');

%%TEST_GLOBALS%%

master_process off;
daemon off;

events {
    use epoll;
}

error_log logs/debug.log debug;

stream {
    proxy_timeout    2s;

    # KCP Server
    server {
        listen       127.0.0.1:%%PORT_20080_UDP%% udp kcp=normal;
        return       200;
    }

    # UDP Server
    server {
        listen       127.0.0.1:%%PORT_20081_UDP%% udp;
        return       200;
    }

    upstream kcp_server {
        server       127.0.0.1:%%PORT_20080_UDP%% udp kcp=normal;
    }

    upstream default_kcp_server {
        server       127.0.0.1:%%PORT_20080_UDP%%;
    }

    upstream real_kcp_server {
        server       127.0.0.1:%%PORT_20082_UDP%%;
    }

    # kcp to kcp
    server {
        listen       127.0.0.1:%%PORT_10080_UDP%% udp kcp=normal;
        proxy_pass   kcp_server;
    }

    # kcp to kcp by default
    server {
        listen       127.0.0.1:%%PORT_10081_UDP%% udp kcp=normal;
        proxy_pass   default_kcp_server;
    }

    # kcp to udp
    server {
        listen       127.0.0.1:%%PORT_10082_UDP%% udp kcp=normal;
        proxy_pass   127.0.0.1:%%PORT_20081_UDP%%;
    }

    # kcp to real kcp server
    server {
        listen      127.0.0.1:%%PORT_10083_UDP%% udp kcp=normal;
        proxy_pass  real_kcp_server;
    }
}

EOF

$t->run();

###############################################################################

SKIP: {
    eval { require KCP };

    skip "KCP not installecd", 4 if $@;

    my ($s, $kcp);

    # kcp to kcp

    $s = dgram('127.0.0.1:' . port(10080));

    $kcp = KCP::new(1, $s);

    $kcp->set_mode("normal");
    $kcp->set_output(\&kcp_output);

    is(kcp_send($kcp, $s, 'a'), '200', 'kcp to kcp');

    # kcp to kcp by default

    $s = dgram('127.0.0.1:' . port(10081));

    $kcp = KCP::new(1, $s);

    $kcp->set_mode("normal");
    $kcp->set_output(\&kcp_output);

    is(kcp_send($kcp, $s, 'a'), '200', 'kcp to kcp');

    # kcp to udp

    $s = dgram('127.0.0.1:' . port(10082));

    $kcp = KCP::new(2, $s);
    $kcp->set_mode("normal");
    $kcp->set_output(\&kcp_output);

    is(kcp_send($kcp, $s, 'a'), '200', 'kcp to udp');

    # kcp to real kcp server
    is(kcp_pair(port(10083), port(20082), 'a'), 'a', 'kcp to real kcp server');
}

###############################################################################

sub kcp_send
{
    my ($kcp, $s, $message) = @_;

    my $interval = $kcp->get_interval;

    $kcp->update(0);

    log2o("kcp send: $message");

    $kcp->send($message);

    my ($data, $input_data);
    while (1)
    {
        $kcp->update($interval);

        $input_data = $s->read(read_timeout => 0.5);

        if ($input_data)
        {
            log2i("input: $input_data");
            $kcp->input($input_data);
        }

        $kcp->recv($data, 65536);
        last if $data;

        $interval += $interval;
    }

    log2i("kcp recv: $data");

    return $data;
}

sub kcp_pair
{
    my ($proxy_port, $server_port, $message) = @_;

    my ($kcp_client, $kcp_server);

    my $server = dgram(
        Proto => "udp",
        LocalAddr => "127.0.0.1:$server_port",
        Reuse => 1
    )
        or die "Can't create listening socket: $!\n";

    my $client = dgram("127.0.0.1:$proxy_port");

    $kcp_client = KCP::new(3, $client);
    $kcp_client->set_mode("normal");
    $kcp_client->set_output(\&kcp_client_output);

    $kcp_server = KCP::new(3, $server);
    $kcp_server->set_mode("normal");
    $kcp_server->set_output(\&kcp_server_output);

    $kcp_client->update(0);
    $kcp_server->update(0);

    # kcp_client send data
    $kcp_client->send($message);
    log2i("kcp_client send: $message");

    my $interval = $kcp_client->get_interval;
    while (1)
    {
        my ($client_data, $server_data, $input_data);

        # update
        $kcp_client->update($interval);
        $kcp_server->update($interval);

        # kcp_client input data
        $input_data = $client->recv(read_timeout => 0.5);

        if ($input_data)
        {
            log2i("client input: $input_data");
            $kcp_client->input($input_data);
        }

        # kcp_server input data
        $input_data = $server->recv(read_timeout => 0.5);

        if ($input_data)
        {
            log2i("server input: $input_data");
            $kcp_server->input($input_data);
        }

        # kcp_client recv data

        $kcp_client->recv($client_data, 65536);
        if ($client_data)
        {
            log2i("kcp_client recv: $client_data");
            return $client_data;
        }

        # kcp_server recv and send data
        $kcp_server->recv($server_data, 65536);

        if ($server_data)
        {
            log2i("kcp_server recv: $server_data");
            $kcp_server->send($server_data);
            log2i("kcp_server send: $server_data");
        }

        $interval += $interval;
    }
}

sub kcp_output
{
    my ($data, $s) = @_;

    log2o("output: $data");

    $s->write($data, write_timeout => 0.5);
}

sub kcp_client_output
{
    my ($data, $s) = @_;

    log2o("client output: $data");

    $s->send($data, write_timeout => 0.5);
}

sub kcp_server_output
{
    my ($data, $s) = @_;

    log2o("server output: $data");

    $s->send($data, write_timeout => 0.5);
}

sub log2i { Test::Nginx::log_core('|| <<', @_); }
sub log2o { Test::Nginx::log_core('|| >>', @_); }
sub log2c { Test::Nginx::log_core('||', @_); }