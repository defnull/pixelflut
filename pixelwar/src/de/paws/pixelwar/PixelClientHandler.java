package de.paws.pixelwar;

import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;

import java.util.HashSet;
import java.util.Set;

public class PixelClientHandler extends SimpleChannelInboundHandler<String> {

	private static Set<PixelClientHandler> clients = new HashSet<>();

	private final NetCanvas canvas;
	private long connected;
	private long c = 0;
	private ChannelHandlerContext channel;

	public PixelClientHandler(NetCanvas canvas) {
		this.canvas = canvas;
	}

	@Override
	public void channelActive(ChannelHandlerContext ctx) throws Exception {
		super.channelActive(ctx);
		this.connected = System.currentTimeMillis();
		channel = ctx;
		synchronized (clients) {
			clients.add(this);
		}
	}

	@Override
	public void channelInactive(ChannelHandlerContext ctx) throws Exception {
		super.channelActive(ctx);
		long passed = System.currentTimeMillis() - connected;
		System.out.print(c * 1000 / passed);
		synchronized (clients) {
			clients.remove(this);
		}
	}

	public void writeIfPossible(String str) {
		if (channel.channel().isWritable()) {
			channel.writeAndFlush(str.trim() + "\n");
		}
	}

	@Override
	protected void channelRead0(ChannelHandlerContext ctx, String msg)
			throws Exception {
		String[] parts = msg.split(" ");
		if (parts.length < 1)
			return;

		String command = parts[0].toUpperCase();

		if (command.equals("PX")) {
			c++;
			if (parts.length == 3) {
				int x = Integer.parseInt(parts[1]);
				int y = Integer.parseInt(parts[2]);
				int c = canvas.getPixel(x, y);
				ctx.write(String.format("PX %d %d %06x\n", x, y, c));
				ctx.flush();
			} else if (parts.length == 4) {
				int x = Integer.parseInt(parts[1]);
				int y = Integer.parseInt(parts[2]);
				int color = (int) Long.parseLong(parts[3], 16);
				if (parts[3].length() == 6)
					color += 0xff000000;
				canvas.setPixel(x, y, color);
			} else {
				ctx.writeAndFlush("ERR\n");
				ctx.close();
			}
		} else if (command.equals("SIZE")) {
			ctx.writeAndFlush(String.format("SIZE %d %d\n", canvas.getWidth(),
					canvas.getHeight()));
		} else if (command.equals("MSG")) {
			String text = command + " "
					+ msg.substring(command.length()).trim();
			for (PixelClientHandler c : clients) {
				if (c != this)
					c.writeIfPossible(text);
			}
		} else {
			ctx.writeAndFlush("ERR\n");
			ctx.close();
		}
	}
}
