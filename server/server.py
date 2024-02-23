# -*- coding: utf-8 -*-

import socket  #导入socket模块
import time #导入time模块
import struct
import subprocess
import signal
import sys
import os
import psutil
from utils import load_json_data, write_json_data
from bilibili_operate import get_play_url_by_bvid, get_play_url_by_keyword, search_bvid_by_keyword, HEADERS

# 与终端的数据交换操作码定义
XNET_REQ_COMMAND = 0x0001
XNET_REQ_NEW_MUSIC = 0x0002
XNET_ACK_MUSIC_NEXT = 0x8001
XNET_ACK_PLAYER_STATE = 0x8002


# 使用ctrl + C 终止程序时，关闭子进程
def signal_handler(signal, frame):
    if p != None:
        p.terminate()
    process = psutil.Process(os.getpid())
    # 获取进程的所有子进程
    children = process.children(recursive=True)
    for child in children:
        child.terminate()
    sys.exit(0)
 
signal.signal(signal.SIGINT,signal_handler)

def send_music_stream():
    # 这里需要使用同一个子进程对象p 所以要用global声明
    global p
    server_music_url = 'http://0.0.0.0:{}/{}'.format(device_config.get('server_url').split(':')[-1], bvid)
    recive_music_url = device_config.get('server_url') + '/' + bvid
    if stop_current_audio == 1:
        if p != None and psutil.pid_exists(p.pid):
            p.terminate()

    if p == None or (p != None and not psutil.pid_exists(p.pid)):
        ffmpeg_command = 'ffmpeg -headers "referer: {}\r\n" -user_agent "{}" \
            -i "{}" -err_detect ignore_err -c:a aac -bufsize 10M -listen 1 -listen_timeout 30000 -ar 44100 -f adts {}'.format(
                HEADERS.get('Referer'), HEADERS.get('User-Agent'), source_play_url, server_music_url)
        print(ffmpeg_command)
        p = subprocess.Popen(ffmpeg_command)
        pyload_size = len(recive_music_url.encode())
        struct_send_format = STRUCT_FROMAT + str(pyload_size) + 's'
        struct_send = struct.pack(struct_send_format, struct_info_tuple[0]+pyload_size, struct_info_tuple[1],
                                struct_info_tuple[2], struct_info_tuple[3], struct_info_tuple[4],
                                XNET_ACK_MUSIC_NEXT, 0, 0, pyload_size, recive_music_url.encode())
        server_socket.sendto(struct_send, client)



# udp服务端端口
PORT = 8000
# udp 数据交换格式定义
STRUCT_FROMAT = '<HHII8sHHHH'

# 创建一个套接字socket对象，用于进行通讯
# socket.AF_INET 指明使用INET地址集，进行网间通讯
# socket.SOCK_DGRAM 指明使用数据协议，即使用传输层的udp协议
server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
address = ("0.0.0.0", PORT)  
server_socket.bind(address)
server_socket.settimeout(10)

# 循环处理过程中需要使用的全局变量
p = None
keyword = ''
bvid = ''
source_play_url = ''
play_music_retry_count = 0

while True:
    try:
        receive_struct_format = STRUCT_FROMAT
        now = time.time()  #获取当前时间
        receive_data, client = server_socket.recvfrom(1024)
        bytes_length = struct.unpack('<H', receive_data[0:2])[0]
        if bytes_length > struct.calcsize(STRUCT_FROMAT):
            receive_struct_format = STRUCT_FROMAT + str(bytes_length-struct.calcsize(STRUCT_FROMAT)) + 's'
        # 数据格式(28, 14120, 0, 0, b'\xe4e\xb8\x0f\xe3h\x00\x00', 2, 0, 0, 0)
        struct_info_tuple = struct.unpack(receive_struct_format, receive_data)
        print(time.strftime('%Y-%m-%d %H:%M:%S',time.localtime(now))) #以指定格式显示时间
        print("来自客户端发送的:", struct_info_tuple)  #打印接收的内容
        
        device_id = struct_info_tuple[4].hex()
        device_config_json_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', str(device_id)+'.json')
        device_config = {
            'name': 'music_box',
            'state_to_set': True,
            'volume': 30,
            'paused': 0,
            'music_url': '',
            'server_url': 'http://192.168.137.1:8180',
            'bvid': '',
            'keyword': '千古'
        }
        if not os.path.exists(device_config_json_path):
            write_json_data(device_config_json_path, device_config)

        device_config = load_json_data(device_config_json_path)

        stop_current_audio = 0
        new_bvid = device_config.get('bvid', '')
        new_keyword = device_config.get('keyword', '')
        
        if new_bvid:
            if new_bvid != bvid:
                stop_current_audio = 1
                bvid = new_bvid
                source_play_url = get_play_url_by_bvid(bvid)
        elif new_keyword:
            if new_keyword != keyword:
                stop_current_audio = 1
                keyword = new_keyword
                bvid = search_bvid_by_keyword(keyword)
                source_play_url = get_play_url_by_bvid(bvid)

        # 按照请求的字段做出处理
        if struct_info_tuple[5] == XNET_REQ_COMMAND:
            if device_config.get('state_to_set'):
                struct_send_format = STRUCT_FROMAT
                struct_send = struct.pack(struct_send_format, struct_info_tuple[0], struct_info_tuple[1],
                                      struct_info_tuple[2], struct_info_tuple[3], struct_info_tuple[4],
                                      XNET_ACK_PLAYER_STATE, device_config.get('volume'), device_config.get('paused'), 0)
                server_socket.sendto(struct_send, client)
            
            if bvid and source_play_url and stop_current_audio == 1:
                send_music_stream()
        
        elif struct_info_tuple[5] == XNET_REQ_NEW_MUSIC:
            if bvid and source_play_url:
                send_music_stream()
            else:
                play_music_retry_count += 1
                print()
                if play_music_retry_count > 30:
                    if p != None and psutil.pid_exists(p.pid):
                        p.terminate()
                    p = 0

    except socket.timeout:
        print("tme out")
    #except Exception as e:
    #    print("报错:{}".format(str(e)))

