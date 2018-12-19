package tcpDemo;
import java.net.*;
import java.io.*;

public class TCPClient {

	public static void main(String[] args) throws Exception {
		// TODO Auto-generated method stub
		Socket cs=new Socket(InetAddress.getByName("127.0.0.1"), 9999);
		BufferedReader bReader=new BufferedReader(new InputStreamReader(cs.getInputStream()));
		PrintWriter pWriter=new PrintWriter(cs.getOutputStream());
		String line=null;
		BufferedReader kbrReader=new BufferedReader(new InputStreamReader(System.in));
		do {
			line=kbrReader.readLine();
			pWriter.println(line);
			pWriter.flush();
			String res=bReader.readLine();
			System.out.println(res);
		} while (!"quit".equalsIgnoreCase(line));
		pWriter.close();
		bReader.close();
		cs.close();
	}

}
