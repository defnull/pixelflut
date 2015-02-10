package de.paws.pixelwar;

import java.awt.AlphaComposite;
import java.awt.Canvas;
import java.awt.Color;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.RenderingHints;
import java.awt.Transparency;
import java.awt.event.ComponentEvent;
import java.awt.event.ComponentListener;
import java.awt.event.KeyEvent;
import java.awt.event.KeyListener;
import java.awt.image.BufferStrategy;
import java.awt.image.BufferedImage;

import javax.swing.JFrame;

public class NetCanvas implements ComponentListener, KeyListener {
	private volatile BufferedImage drawBuffer;
	private volatile BufferedImage overlayBuffer;
	private volatile BufferedImage paintBuffer;

	private BufferStrategy strategy;
	private final Thread refresher;
	private final JFrame frame;
	private final Canvas canvas;

	public NetCanvas() {
		frame = new JFrame();
		canvas = new Canvas();
		resizeBuffer(1024, 1024);

		frame.addComponentListener(this);
		canvas.addKeyListener(this);

		canvas.setSize(800, 600);
		frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
		frame.setTitle("Pixelflut");
		// frame.setUndecorated(true);
		frame.setResizable(true);
		// frame.setAlwaysOnTop(true);
		frame.add(canvas);
		frame.pack();
		frame.setVisible(true);

		refresher = new Thread(new Runnable() {
			@Override
			public void run() {
				try {
					while (true) {
						draw();
						Thread.sleep(1000 / 30);
					}
				} catch (final InterruptedException e) {
				}
			}
		});
		refresher.start();

	}

	private void blit(final BufferedImage source, final BufferedImage target) {
		final Graphics g = target.getGraphics();
		g.drawImage(source, 0, 0, null);
		g.dispose();
	}

	synchronized private void resizeBuffer(final int w, final int h) {
		BufferedImage newBuffer;
		int mw = w, mh = h;

		if (drawBuffer != null) {
			mw = Math.max(w, drawBuffer.getWidth());
			mh = Math.max(h, drawBuffer.getHeight());
			if (mw > w && mh > h) {
				return;
			}
		}

		newBuffer = frame.getGraphicsConfiguration().createCompatibleImage(w,
				h, Transparency.OPAQUE);
		if (drawBuffer != null) {
			blit(drawBuffer, newBuffer);
		}
		drawBuffer = newBuffer;

		overlayBuffer = frame.getGraphicsConfiguration().createCompatibleImage(
				w, h, Transparency.TRANSLUCENT);

		paintBuffer = frame.getGraphicsConfiguration().createCompatibleImage(w,
				h, Transparency.OPAQUE);
	}

	private void drawOverlay() {
		final Graphics2D g = overlayBuffer.createGraphics();
		g.setComposite(AlphaComposite.Clear);
		g.fillRect(0, 0, overlayBuffer.getWidth(), overlayBuffer.getHeight());
		g.setComposite(AlphaComposite.SrcOver);
		g.setRenderingHint(RenderingHints.KEY_ANTIALIASING,
				RenderingHints.VALUE_ANTIALIAS_ON);
		g.drawString(String.format("Pixelflut"), 2, 10);
		g.dispose();
	}

	synchronized public void draw() {
		if (!frame.isVisible()) {
			return;
		}
		drawOverlay();
		// Prepare composite buffer
		final Graphics2D g = paintBuffer.createGraphics();
		g.drawImage(drawBuffer, 0, 0, null);
		g.setComposite(AlphaComposite.getInstance(AlphaComposite.SRC_OVER));
		g.drawImage(overlayBuffer, 0, 0, null);
		g.dispose();

		// Blit into double buffer
		final Graphics g2 = strategy.getDrawGraphics();
		g2.drawImage(paintBuffer, 0, 0, null);
		g2.dispose();
		strategy.show();
	}

	public void setPixel(final int x, final int y, final int argb) {
		// Below this values 8bit color channels are nulled.
		final BufferedImage img = drawBuffer;

		if (x >= 0 && x < img.getWidth() && y >= 0 && y < img.getHeight()) {
			final int alpha = (argb >>> 24) % 256;
			int rgb = argb & 0xffffff;

			if (alpha < 1) {
				return;
			}

			if (alpha < 255) {
				final int target = img.getRGB(x, y);
				int r, g, b;

				r = ((target >>> 16) & 0xff) * (255 - alpha);
				g = ((target >>> 8) & 0xff) * (255 - alpha);
				b = ((target >>> 0) & 0xff) * (255 - alpha);

				r += ((argb >>> 16) & 0xff) * alpha;
				g += ((argb >>> 8) & 0xff) * alpha;
				b += ((argb >>> 0) & 0xff) * alpha;

				rgb = 0xff << 24;
				rgb += (r / 255) << 16;
				rgb += (g / 255) << 8;
				rgb += (b / 255) << 0;
			}

			img.setRGB(x, y, rgb);
		}
	}

	public int getPixel(final int x, final int y) {
		final BufferedImage img = drawBuffer;
		if (x >= 0 && x < img.getWidth() && y >= 0 && y < img.getHeight()) {
			return img.getRGB(x, y) & 0x00ffffff;
		}
		return 0;
	}

	public int getWidth() {
		return drawBuffer.getWidth();
	}

	public int getHeight() {
		return drawBuffer.getHeight();
	}

	@Override
	public void keyReleased(final KeyEvent e) {
		if (e.getKeyChar() == 'c') {
			final Graphics g = drawBuffer.getGraphics();
			g.setColor(Color.BLACK);
			g.fillRect(0, 0, drawBuffer.getWidth(), drawBuffer.getHeight());
			g.dispose();
		} else if (e.getKeyChar() == 'q'
				|| e.getKeyCode() == KeyEvent.VK_ESCAPE) {
			System.exit(0);
		}
	}

	@Override
	public void keyPressed(final KeyEvent e) {
	}

	@Override
	public void keyTyped(final KeyEvent e) {
	}

	@Override
	public void componentResized(final ComponentEvent e) {
		resizeBuffer(canvas.getWidth(), canvas.getHeight());
	}

	@Override
	public void componentMoved(final ComponentEvent e) {
	}

	@Override
	public void componentShown(final ComponentEvent e) {
		canvas.createBufferStrategy(2);
		strategy = canvas.getBufferStrategy();
	}

	@Override
	public void componentHidden(final ComponentEvent e) {
	}

}
