#!/bin/bash
#
# driver.sh - Proxy Lab 的简易自动评分器。
#     执行基本的正确性检查，判断代码是否表现为一个
#     支持并发的缓存 proxy。
#
#     David O'Hallaron, Carnegie Mellon University
#     updated: 2/8/2016
#
#     usage: ./driver.sh
#

# 分值
MAX_BASIC=40
MAX_CONCURRENCY=15
MAX_CACHE=15

# 各种常量
HOME_DIR=`pwd`
PROXY_DIR="./.proxy"
NOPROXY_DIR="./.noproxy"
TIMEOUT=5
MAX_RAND=63000
PORT_START=1024
PORT_MAX=65000
MAX_PORT_TRIES=10

# basic 测试所需的文本和二进制文件列表
BASIC_LIST="home.html
            csapp.c
            tiny.c
            godzilla.jpg
            tiny"

# cache 测试所需的文本文件列表
CACHE_LIST="tiny.c
            home.html
            csapp.c"

# 各测试中将获取的文件
FETCH_FILE="home.html"

#####
# 辅助函数
#

#
# download_proxy - 通过 proxy 从源服务器下载文件
# usage: download_proxy <testdir> <filename> <origin_url> <proxy_url>
#
function download_proxy {
    cd $1
    curl --max-time ${TIMEOUT} --silent --proxy $4 --output $2 $3
    (( $? == 28 )) && echo "Error: Fetch timed out after ${TIMEOUT} seconds"
    cd $HOME_DIR
}

#
# download_noproxy - 直接从源服务器下载文件
# usage: download_noproxy <testdir> <filename> <origin_url>
#
function download_noproxy {
    cd $1
    curl --max-time ${TIMEOUT} --silent --output $2 $3 
    (( $? == 28 )) && echo "Error: Fetch timed out after ${TIMEOUT} seconds"
    cd $HOME_DIR
}

#
# clear_dirs - 清空下载目录
#
function clear_dirs {
    rm -rf ${PROXY_DIR}/*
    rm -rf ${NOPROXY_DIR}/*
}

#
# wait_for_port_use - 轮询等待作为参数传入的 TCP 端口号被实际占用。
#     5 秒后超时。
#
function wait_for_port_use() {
    timeout_count="0"
    portsinuse=`netstat --numeric-ports --numeric-hosts -a --protocol=tcpip \
        | grep tcp | cut -c21- | cut -d':' -f2 | cut -d' ' -f1 \
        | grep -E "[0-9]+" | uniq | tr "\n" " "`

    echo "${portsinuse}" | grep -wq "${1}"
    while [ "$?" != "0" ]
    do
        timeout_count=`expr ${timeout_count} + 1`
        if [ "${timeout_count}" == "${MAX_PORT_TRIES}" ]; then
            kill -ALRM $$
        fi

        sleep 1
        portsinuse=`netstat --numeric-ports --numeric-hosts -a --protocol=tcpip \
            | grep tcp | cut -c21- | cut -d':' -f2 | cut -d' ' -f1 \
            | grep -E "[0-9]+" | uniq | tr "\n" " "`
        echo "${portsinuse}" | grep -wq "${1}"
    done
}


#
# free_port - 返回一个可用的未被占用的 TCP 端口
#
function free_port {
    # 在 [PORT_START, PORT_START+MAX_RAND] 范围内生成随机端口。
    # 这是为了避免多名学生在同一台机器上运行 driver 时发生冲突。
    port=$((( RANDOM % ${MAX_RAND}) + ${PORT_START}))

    while [ TRUE ] 
    do
        portsinuse=`netstat --numeric-ports --numeric-hosts -a --protocol=tcpip \
            | grep tcp | cut -c21- | cut -d':' -f2 | cut -d' ' -f1 \
            | grep -E "[0-9]+" | uniq | tr "\n" " "`

        echo "${portsinuse}" | grep -wq "${port}"
        if [ "$?" == "0" ]; then
            if [ $port -eq ${PORT_MAX} ]
            then
                echo "-1"
                return
            fi
            port=`expr ${port} + 1`
        else
            echo "${port}"
            return
        fi
    done
}


#######
# 主程序
#######

######
# 验证所有预期文件存在且权限正确
#

# 终止当前用户拥有的任何残留 proxy 或 tiny 服务器
killall -q proxy tiny nop-server.py 2> /dev/null

# 确保存在 Tiny 目录
if [ ! -d ./tiny ]
then 
    echo "Error: ./tiny directory not found."
    exit
fi

# 如果没有 Tiny 可执行文件，尝试构建它
if [ ! -x ./tiny/tiny ]
then 
    echo "Building the tiny executable."
    (cd ./tiny; make)
    echo ""
fi

# 确保所需的 Tiny 文件都存在
if [ ! -x ./tiny/tiny ]
then 
    echo "Error: ./tiny/tiny not found or not an executable file."
    exit
fi
for file in ${BASIC_LIST}
do
    if [ ! -e ./tiny/${file} ]
    then
        echo "Error: ./tiny/${file} not found."
        exit
    fi
done

# 确保存在可执行的 proxy
if [ ! -x ./proxy ]
then 
    echo "Error: ./proxy not found or not an executable file. Please rebuild your proxy and try again."
    exit
fi

# 确保存在可执行的 nop-server.py 文件
if [ ! -x ./nop-server.py ]
then 
    echo "Error: ./nop-server.py not found or not an executable file."
    exit
fi

# 如有需要则创建测试目录
if [ ! -d ${PROXY_DIR} ]
then
    mkdir ${PROXY_DIR}
fi

if [ ! -d ${NOPROXY_DIR} ]
then
    mkdir ${NOPROXY_DIR}
fi

# 添加超时处理器，生成有意义的超时消息
trap 'echo "Timeout waiting for the server to grab the port reserved for it"; kill $$' ALRM

#####
# Basic 测试
#
echo "*** Basic ***"

# 启动 Tiny Web 服务器
tiny_port=$(free_port)
echo "Starting tiny on ${tiny_port}"
cd ./tiny
./tiny ${tiny_port}   &> /dev/null  &
tiny_pid=$!
cd ${HOME_DIR}

# 等待 tiny 服务就绪
wait_for_port_use "${tiny_port}"

# 启动 proxy
proxy_port=$(free_port)
echo "Starting proxy on ${proxy_port}"
./proxy ${proxy_port}  &> /dev/null &
proxy_pid=$!

# 等待 proxy 服务就绪
wait_for_port_use "${proxy_port}"


# 开始测试：分别通过直接连接和 proxy 两种方式从 Tiny 获取
# 文本和二进制文件，然后比较结果。
numRun=0
numSucceeded=0
for file in ${BASIC_LIST}
do
    numRun=`expr $numRun + 1`
    echo "${numRun}: ${file}"
    clear_dirs

    # 通过 proxy 获取
    echo "   Fetching ./tiny/${file} into ${PROXY_DIR} using the proxy"
    download_proxy $PROXY_DIR ${file} "http://localhost:${tiny_port}/${file}" "http://localhost:${proxy_port}"

    # 直接从 Tiny 获取
    echo "   Fetching ./tiny/${file} into ${NOPROXY_DIR} directly from Tiny"
    download_noproxy $NOPROXY_DIR ${file} "http://localhost:${tiny_port}/${file}"

    # 比较两个文件
    echo "   Comparing the two files"
    diff -q ${PROXY_DIR}/${file} ${NOPROXY_DIR}/${file} &> /dev/null
    if [ $? -eq 0 ]; then
        numSucceeded=`expr ${numSucceeded} + 1`
        echo "   Success: Files are identical."
    else
        echo "   Failure: Files differ."
    fi
done

echo "Killing tiny and proxy"
kill $tiny_pid 2> /dev/null
wait $tiny_pid 2> /dev/null
kill $proxy_pid 2> /dev/null
wait $proxy_pid 2> /dev/null

basicScore=`expr ${MAX_BASIC} \* ${numSucceeded} / ${numRun}`

echo "basicScore: $basicScore/${MAX_BASIC}"


######
# Concurrency 测试
#

echo ""
echo "*** Concurrency ***"

# 启动 Tiny Web 服务器
tiny_port=$(free_port)
echo "Starting tiny on port ${tiny_port}"
cd ./tiny
./tiny ${tiny_port} &> /dev/null &
tiny_pid=$!
cd ${HOME_DIR}

# 等待 tiny 服务就绪
wait_for_port_use "${tiny_port}"

# 启动 proxy
proxy_port=$(free_port)
echo "Starting proxy on port ${proxy_port}"
./proxy ${proxy_port} &> /dev/null &
proxy_pid=$!

# 等待 proxy 服务就绪
wait_for_port_use "${proxy_port}"

# 运行一个特殊的阻塞式 nop-server，它从不响应请求
nop_port=$(free_port)
echo "Starting the blocking NOP server on port ${nop_port}"
./nop-server.py ${nop_port} &> /dev/null &
nop_pid=$!

# 等待 nop 服务器就绪
wait_for_port_use "${nop_port}"

# 尝试通过 proxy 从阻塞式 nop-server 获取一个文件
clear_dirs
echo "Trying to fetch a file from the blocking nop-server"
download_proxy $PROXY_DIR "nop-file.txt" "http://localhost:${nop_port}/nop-file.txt" "http://localhost:${proxy_port}" &

# 直接从 Tiny 获取
echo "Fetching ./tiny/${FETCH_FILE} into ${NOPROXY_DIR} directly from Tiny"
download_noproxy $NOPROXY_DIR ${FETCH_FILE} "http://localhost:${tiny_port}/${FETCH_FILE}"

# 通过 proxy 获取
echo "Fetching ./tiny/${FETCH_FILE} into ${PROXY_DIR} using the proxy"
download_proxy $PROXY_DIR ${FETCH_FILE} "http://localhost:${tiny_port}/${FETCH_FILE}" "http://localhost:${proxy_port}"

# 检查 proxy 获取是否成功
echo "Checking whether the proxy fetch succeeded"
diff -q ${PROXY_DIR}/${FETCH_FILE} ${NOPROXY_DIR}/${FETCH_FILE} &> /dev/null
if [ $? -eq 0 ]; then
    concurrencyScore=${MAX_CONCURRENCY}
    echo "Success: Was able to fetch tiny/${FETCH_FILE} from the proxy."
else
    concurrencyScore=0
    echo "Failure: Was not able to fetch tiny/${FETCH_FILE} from the proxy."
fi

# 清理
echo "Killing tiny, proxy, and nop-server"
kill $tiny_pid 2> /dev/null
wait $tiny_pid 2> /dev/null
kill $proxy_pid 2> /dev/null
wait $proxy_pid 2> /dev/null
kill $nop_pid 2> /dev/null
wait $nop_pid 2> /dev/null

echo "concurrencyScore: $concurrencyScore/${MAX_CONCURRENCY}"

#####
# Cache 测试
#
echo ""
echo "*** Cache ***"

# 启动 Tiny Web 服务器
tiny_port=$(free_port)
echo "Starting tiny on port ${tiny_port}"
cd ./tiny
./tiny ${tiny_port} &> /dev/null &
tiny_pid=$!
cd ${HOME_DIR}

# 等待 tiny 服务就绪
wait_for_port_use "${tiny_port}"

# 启动 proxy
proxy_port=$(free_port)
echo "Starting proxy on port ${proxy_port}"
./proxy ${proxy_port} &> /dev/null &
proxy_pid=$!

# 等待 proxy 服务就绪
wait_for_port_use "${proxy_port}"

# 通过 proxy 从 tiny 获取一些文件
clear_dirs
for file in ${CACHE_LIST}
do
    echo "Fetching ./tiny/${file} into ${PROXY_DIR} using the proxy"
    download_proxy $PROXY_DIR ${file} "http://localhost:${tiny_port}/${file}" "http://localhost:${proxy_port}"
done

# 终止 Tiny
echo "Killing tiny"
kill $tiny_pid 2> /dev/null
wait $tiny_pid 2> /dev/null

# 现在尝试获取其中一个文件的缓存副本。
echo "Fetching a cached copy of ./tiny/${FETCH_FILE} into ${NOPROXY_DIR}"
download_proxy $NOPROXY_DIR ${FETCH_FILE} "http://localhost:${tiny_port}/${FETCH_FILE}" "http://localhost:${proxy_port}"

# 通过与 tiny 目录中的原始文件比较，检查 proxy 获取是否成功
diff -q ./tiny/${FETCH_FILE} ${NOPROXY_DIR}/${FETCH_FILE}  &> /dev/null
if [ $? -eq 0 ]; then
    cacheScore=${MAX_CACHE}
    echo "Success: Was able to fetch tiny/${FETCH_FILE} from the cache."
else
    cacheScore=0
    echo "Failure: Was not able to fetch tiny/${FETCH_FILE} from the proxy cache."
fi

# 终止 proxy
echo "Killing proxy"
kill $proxy_pid 2> /dev/null
wait $proxy_pid 2> /dev/null

echo "cacheScore: $cacheScore/${MAX_CACHE}"

# 输出总分
totalScore=`expr ${basicScore} + ${cacheScore} + ${concurrencyScore}`
maxScore=`expr ${MAX_BASIC} + ${MAX_CACHE} + ${MAX_CONCURRENCY}`
echo ""
echo "totalScore: ${totalScore}/${maxScore}"
exit

