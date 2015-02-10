package de.paws.pixelwar;

import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;
import io.netty.handler.traffic.TrafficCounter;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import org.apache.commons.lang3.StringUtils;

public class PixelClientHandler extends SimpleChannelInboundHandler<String> {

	public interface CommandHandler {
		public void handle(PixelClientHandler client, String[] args);
	}

	private static Set<PixelClientHandler> clients = new HashSet<>();

	private final NetCanvas canvas;
	private ChannelHandlerContext channelContext;
	private final Set<String> subscriptions = new HashSet<>();
	private final Map<String, CommandHandler> handlers = new HashMap<>();

	private long connectedTime;
	private long pixelCount = 0;
	private long missedMessages = 0;

	public PixelClientHandler(final NetCanvas canvas) {
		this.canvas = canvas;
		final TrafficCounter tc = new TrafficCounter(null, null, null,
				connectedTime);
	}

	public void installHandler(final String command,
			final CommandHandler handler) {
		handlers.put(command.toUpperCase(), handler);
	}

	@Override
	public void channelActive(final ChannelHandlerContext ctx) throws Exception {
		super.channelActive(ctx);
		connectedTime = System.currentTimeMillis();
		channelContext = ctx;
		synchronized (clients) {
			clients.add(this);
		}
	}

	@Override
	public void channelInactive(final ChannelHandlerContext ctx)
			throws Exception {
		super.channelActive(ctx);
		final long passed = System.currentTimeMillis() - connectedTime;
		synchronized (clients) {
			clients.remove(this);
		}
	}

	public void writeIfPossible(final String str) {
		if (channelContext.channel().isWritable()) {
			channelContext.write(str + "\n");
		} else {
			missedMessages++;
		}
	}

	private void writeChannel(final String channel, final String message) {
		if (subscriptions.contains(channel) || subscriptions.contains("*")) {
			writeIfPossible(message);
		}
	}

	public void write(final String str) {
		channelContext.writeAndFlush(str + "\n");
	}

	public void error(final String msg) {
		write("ERR " + msg + "\n");
		channelContext.close();
	}

	@Override
	protected void channelRead0(final ChannelHandlerContext ctx,
			final String msg) throws Exception {
		final int split = msg.indexOf(" ");
		String command;
		String data;
		if (split == -1) {
			command = msg.toUpperCase();
			data = "";
		} else {
			command = msg.substring(0, split).toUpperCase();
			data = msg.substring(split + 1).trim();
		}

		switch (command) {
		case ("PX"):
			handle_PX(ctx, data);
			break;
		case ("SIZE"):
			handle_SIZE(ctx, data);
			break;
		case ("PUB"):
			handle_PUB(ctx, data);
			break;
		case ("SUB"):
			handle_SUB(ctx, data);
			break;
		case ("HELP"):
			handle_HELP(ctx, data);
			break;
		default:
			error("Unknown command");
		}
	}

	private void handle_HELP(final ChannelHandlerContext ctx, final String data) {
		writeIfPossible("HELP Commands:\n" + "HELP  SIZE\n"
				+ "HELP  PX <x> <y>\n" + "HELP  PX <x> <y> <rrggbb[aa]>\n"
				+ "HELP  SUB\n" + "HELP  SUB [-]<channel>\n"
				+ "HELP  PUB <channel> <message>\n");
	}

	private void handle_SUB(final ChannelHandlerContext ctx, final String data) {
		if (data.startsWith("-")) {
			final String channel = data.substring(1);
			if (!subscriptions.remove(channel.toLowerCase())) {
				error("Not subscribed to this channel.");
			}
		} else if (data.length() > 0) {
			final String channel = data.toLowerCase();
			if (!channel.matches("^[a-zA-Z0-9_]+$")) {
				error("Invalid channel name");
			} else if (subscriptions.size() >= 16) {
				error("Too many subscriptions");
			} else {
				subscriptions.add(channel);
			}
		}

		write("SUB " + StringUtils.join(subscriptions, " "));
	}

	private void handle_PUB(final ChannelHandlerContext ctx, final String data) {
		final String[] parts = data.split(" ", 1);
		if (parts.length != 2) {
			error("Usage: PUB <channel> <message>");
			return;
		}

		final String channel = parts[0].toLowerCase();
		final String message = "PUB " + channel + " " + parts[1].trim();

		for (final PixelClientHandler c : clients) {
			if (c != this) {
				c.writeChannel(channel, message);
			}
		}
	}

	private void handle_SIZE(final ChannelHandlerContext ctx, final String data) {
		if (data.length() > 0) {
			error("SIZE");
		}
		write(String
				.format("SIZE %d %d", canvas.getWidth(), canvas.getHeight()));
	}

	private void handle_PX(final ChannelHandlerContext ctx, final String data) {
		final String[] args = data.split(" ");
		try {
			pixelCount++;
			if (args.length == 2) {
				final int x = Integer.parseInt(args[0]);
				final int y = Integer.parseInt(args[1]);
				final int c = canvas.getPixel(x, y);
				write(String.format("PX %d %d %06x", x, y, c));
			} else if (args.length == 3) {
				final int x = Integer.parseInt(args[0]);
				final int y = Integer.parseInt(args[1]);
				int color = (int) Long.parseLong(args[2], 16);
				if (args[2].length() == 6) {
					color += 0xff000000;
				}
				canvas.setPixel(x, y, color);
			} else {
				error("Usage: PX x y [rrggbb[aa]]");
			}
		} catch (final NumberFormatException e) {
			error("Usage: PX x y [rrggbb[aa]]");
		}
	}
}
