import asyncio
import time
import argparse
import logging

# 配置日志
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

CONCURRENT_CLIENTS = 5000
KEEPALIVE_TIME = 5
SUCCESSFUL_CONNECTIONS = 0
FAILED_CONNECTIONS = 0
HOST = '127.0.0.1'
PORT = 9090

async def test_client():
    global SUCCESSFUL_CONNECTIONS, FAILED_CONNECTIONS
    try:
        # 创建 socket 对象
        reader, writer = await asyncio.open_connection(HOST, PORT)
        logging.debug(f"Connected to {HOST}:{PORT}")
        
        # 保持连接一段时间
        await asyncio.sleep(KEEPALIVE_TIME)
        
        # 关闭连接
        writer.close()
        await writer.wait_closed()
        SUCCESSFUL_CONNECTIONS += 1
        logging.debug("Connection closed")

    except Exception as e:
        logging.error(f"Connection error: {e}")
        FAILED_CONNECTIONS += 1

async def main():
    # 创建客户端任务
    tasks = []
    for _ in range(CONCURRENT_CLIENTS):
        tasks.append(asyncio.create_task(test_client()))

    # 等待所有任务完成
    await asyncio.gather(*tasks)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Test server concurrency.')
    parser.add_argument('--clients', type=int, default=CONCURRENT_CLIENTS, help='Number of concurrent clients')
    parser.add_argument('--host', type=str, default=HOST, help='Server host')
    parser.add_argument('--port', type=int, default=PORT, help='Server port')
    parser.add_argument('--time', type=int, default=KEEPALIVE_TIME, help='Connection keepalive time in seconds')

    args = parser.parse_args()
    CONCURRENT_CLIENTS = args.clients
    HOST = args.host
    PORT = args.port
    KEEPALIVE_TIME = args.time

    start_time = time.time()

    # Run the main coroutine
    asyncio.run(main())

    end_time = time.time()

    print(f"Test completed in {end_time - start_time:.2f} seconds")
    print(f"Successful connections: {SUCCESSFUL_CONNECTIONS}")
    print(f"Failed connections: {FAILED_CONNECTIONS}")