package server;

import java.net.*;

public class TCPServer {

	public static void Server() throws Exception {
		System.out.println("服务器在端口号9999等待连接...");
		@SuppressWarnings("resource")
		ServerSocket ss = new ServerSocket(9999);
		while (true) {
			Socket cs = ss.accept();
			ServerThread st=new ServerThread(cs);
			st.start();
		}
	}

	public static void main(String[] args) {
		// TODO Auto-generated method stub
		try {
			Server();
		} catch (Exception e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}

}
