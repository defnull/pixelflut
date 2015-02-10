package de.paws.pixelwar;

import io.netty.bootstrap.ServerBootstrap;
import io.netty.channel.ChannelFuture;
import io.netty.channel.ChannelHandlerAdapter;
import io.netty.channel.ChannelInitializer;
import io.netty.channel.ChannelOption;
import io.netty.channel.ChannelPipeline;
import io.netty.channel.EventLoopGroup;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.SocketChannel;
import io.netty.channel.socket.nio.NioServerSocketChannel;
import io.netty.handler.codec.DelimiterBasedFrameDecoder;
import io.netty.handler.codec.Delimiters;
import io.netty.handler.codec.string.StringDecoder;
import io.netty.handler.codec.string.StringEncoder;
import io.netty.handler.logging.LogLevel;
import io.netty.handler.logging.LoggingHandler;

public class PixelServer extends ChannelHandlerAdapter {

	private final NetCanvas canvas;
	private final int port;

	public PixelServer(final int port) {
		this.port = port;
		canvas = new NetCanvas();
	}

	public void run() throws InterruptedException {
		final EventLoopGroup bossGroup = new NioEventLoopGroup(1);
		final EventLoopGroup workerGroup = new NioEventLoopGroup();
		try {
			final ServerBootstrap b = new ServerBootstrap();
			b.group(bossGroup, workerGroup)
					.channel(NioServerSocketChannel.class)
					.option(ChannelOption.SO_BACKLOG, 100)
					.handler(new LoggingHandler(LogLevel.INFO))
					.childHandler(new ChannelInitializer<SocketChannel>() {
						@Override
						public void initChannel(final SocketChannel ch)
								throws Exception {
							final ChannelPipeline p = ch.pipeline();

							// p.addLast("logger", new LoggingHandler(
							// LogLevel.DEBUG));
							p.addLast("framer", new DelimiterBasedFrameDecoder(
									128, Delimiters.lineDelimiter()));
							p.addLast("decoder", new StringDecoder());
							p.addLast("encoder", new StringEncoder());

							// and then business logic.
							p.addLast("handler", new PixelClientHandler(canvas));
						}
					});

			// Start the server.
			final ChannelFuture f = b.bind(port).sync();

			// Wait until the server socket is closed.
			f.channel().closeFuture().sync();
		} finally {
			// Shut down all event loops to terminate all threads.
			bossGroup.shutdownGracefully();
			workerGroup.shutdownGracefully();
		}
	}

	public static void main(final String[] args) throws InterruptedException {
		new PixelServer(8080).run();
	}

}
