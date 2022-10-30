#!/bin/sh

#################################### global variable

G_PROJECT_DIR=`cd $( dirname $0 ); pwd`
G_TOOLS_DIR=$G_PROJECT_DIR/tools-dev

G_CONF_VERSION=1.0

#################################### function

. $G_TOOLS_DIR/base_for_bash.func

function usage
{
    echo "Usage: $0 [OPTIONS]
OPTIONS:
  -h            : help
  -v <VERSION>  : set version, format is 'master.minor', such as '1.0'" >& 2
}

#################################### main

while getopts :hv: opt
do
    case $opt in
        h)
            usage
            exit 0
            ;;
        v)
            G_CONF_VERSION=$OPTARG
            ;;
        '?')
            error_msg "$0: invalid option -$OPTARG"
            usage
            exit 1
            ;;
    esac
done

shift $(($OPTIND - 1))

docker build --rm -t homqyy/hengine:$G_CONF_VERSION .