package de.paws.pixelwar;

import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;

public class PixelClientHandler extends SimpleChannelInboundHandler<String> {

	private final Canvas canvas;

	public PixelClientHandler(Canvas canvas) {
		this.canvas = canvas;
	}

	@Override
	protected void channelRead0(ChannelHandlerContext ctx, String msg)
			throws Exception {
		String[] parts = msg.split(" ");
		if (parts.length < 1)
			return;

		String command = parts[0].toUpperCase();

		if (command.equals("PX")) {
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
			}
		} else if (command.equals("SIZE")) {
			ctx.write(String.format("SIZE %d %d\n", canvas.getWidth(),
					canvas.getHeight()));
			ctx.flush();
		} else if (command.equals("TILE") && parts.length == 3) {
		}

	}
}
