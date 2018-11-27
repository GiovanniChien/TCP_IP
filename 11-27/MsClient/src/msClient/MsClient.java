package msClient;
import java.net.*;
import java.text.SimpleDateFormat;
//import java.util.Scanner;
import java.util.Date;

public class MsClient {

	public static void main(String[] args) throws Exception {
		// TODO Auto-generated method stub
		DatagramSocket ds = new DatagramSocket();
		byte[] rbuf = new byte[16 * 1024];
		DatagramPacket rdp = new DatagramPacket(rbuf, 16 * 1024);
		//Scanner sc = new Scanner(System.in);
		//String line = null;
		for(int i=0;i<10;i++){
			//line = sc.nextLine();
			SimpleDateFormat sdf=new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");
			String time=sdf.format(new Date());
			DatagramPacket sdp = new DatagramPacket(time.getBytes(), time.getBytes().length,
					InetAddress.getByName("226.6.6.6"), 9999);
			ds.send(sdp);
			Thread.sleep(3000);
		} 
		//sc.close();
		ds.close();
	}

}