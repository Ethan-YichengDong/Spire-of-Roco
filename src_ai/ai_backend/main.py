import socket
import json
from agent import process_game_state

# 开启并在本地建立 TCP Server 服务，充当与 C 核心引擎通讯的 AI 后端
def start_server():
    host = '127.0.0.1'
    port = 8888

    # 创建一个标准的 IPv4 TCP Socket
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((host, port))
        s.listen()
        print(f"AI 后端服务器已经启动，正在监听以提供服务 {host}:{port}")

        while True:
            # 阻塞并接收到来的游戏引擎连接请求
            conn, addr = s.accept()
            with conn:
                # 获取来自游戏的 JSON 状态切片缓存数据
                data = conn.recv(1024)
                if not data:
                    continue
                try:
                    payload = data.decode('utf-8')
                    state_dict = json.loads(payload)
                    print(f"接收到新的游戏对战状态: {state_dict}")
                    
                    # 经过特定的逻辑处理模块（或大型模型）进行决策生成
                    action = process_game_state(state_dict)
                    
                    # 将决策转换为 JSON 字符串结构并编码发送回 C 游戏引擎
                    response_json = json.dumps(action)
                    print(f"向引擎发送行动响应: {response_json}")
                    conn.sendall(response_json.encode('utf-8'))
                except json.JSONDecodeError:
                    print("错误: 收到非法或不完整的 JSON 字典数据。")
                    # 发送错误的 Fallback 降级数据给客户端，避免死锁
                    conn.sendall(b'{"type": 0, "card_hand_idx": 0, "switch_to_idx": -1}')

if __name__ == "__main__":
    start_server()
