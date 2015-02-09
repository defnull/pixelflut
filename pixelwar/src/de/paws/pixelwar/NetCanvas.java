package de.paws.pixelwar;

import java.awt.AlphaComposite;
import java.awt.Canvas;
import java.awt.Color;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.RenderingHints;
import java.awt.Transparency;
import java.awt.event.ComponentEvent;
import java.awt.event.KeyEvent;
import java.awt.image.BufferStrategy;
import java.awt.image.BufferedImage;

import javax.swing.JFrame;

public class NetCanvas {
	private volatile BufferedImage drawBuffer;
	private volatile BufferedImage overlayBuffer;
	private volatile BufferedImage paintBuffer;

	private final BufferStrategy strategy;
	private final Thread refresher;
	private final JFrame frame;
	private final Canvas canvas;

	public NetCanvas() {

		frame = new JFrame() {
			private static final long serialVersionUID = 1L;

			@Override
			protected void processKeyEvent(KeyEvent e) {
				super.processKeyEvent(e);
				if (e.getKeyChar() == 'c') {
					Graphics g = drawBuffer.getGraphics();
					g.setColor(Color.BLACK);
					g.fillRect(0, 0, drawBuffer.getWidth(),
							drawBuffer.getHeight());
					g.dispose();
				}
				if (e.getKeyChar() == 'q'
						|| e.getKeyCode() == KeyEvent.VK_ESCAPE) {
					System.exit(0);
				}
			}

			@Override
			protected void processComponentEvent(ComponentEvent e) {
				super.processComponentEvent(e);
				if (e.getID() == ComponentEvent.COMPONENT_RESIZED) {
					resizeBuffer(getWidth(), getHeight());
				}
			}
		};

		canvas = new Canvas();
		canvas.setSize(800, 600);

		frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
		frame.setTitle("Pixelflut");
		// frame.setUndecorated(true);
		frame.setResizable(true);
		// frame.setAlwaysOnTop(true);

		frame.add(canvas);
		frame.pack();
		frame.setVisible(true);

		canvas.createBufferStrategy(2);
		strategy = canvas.getBufferStrategy();
		resizeBuffer(1024, 1024);

		refresher = new Thread(new Runnable() {
			@Override
			public void run() {
				try {
					while (true) {
						draw();
						Thread.sleep(1000 / 30);
					}
				} catch (InterruptedException e) {
				}
			}
		});
		refresher.start();
	}

	private void blit(BufferedImage source, BufferedImage target) {
		Graphics g = target.getGraphics();
		g.drawImage(source, 0, 0, null);
		g.dispose();
	}

	synchronized private void resizeBuffer(int w, int h) {
		BufferedImage newBuffer;
		int mw = w, mh = h;

		if (drawBuffer != null) {
			mw = Math.max(w, drawBuffer.getWidth());
			mh = Math.max(h, drawBuffer.getHeight());
			if (mw > w && mh > h)
				return;
		}

		newBuffer = frame.getGraphicsConfiguration().createCompatibleImage(w,
				h, Transparency.OPAQUE);
		if (drawBuffer != null)
			blit(drawBuffer, newBuffer);
		drawBuffer = newBuffer;

		overlayBuffer = frame.getGraphicsConfiguration().createCompatibleImage(
				w, h, Transparency.TRANSLUCENT);

		paintBuffer = frame.getGraphicsConfiguration().createCompatibleImage(w,
				h, Transparency.OPAQUE);
	}

	private void drawOverlay() {
		Graphics2D g = overlayBuffer.createGraphics();
		g.setComposite(AlphaComposite.Clear);
		g.fillRect(0, 0, overlayBuffer.getWidth(), overlayBuffer.getHeight());
		g.setComposite(AlphaComposite.SrcOver);
		g.setRenderingHint(RenderingHints.KEY_ANTIALIASING,
				RenderingHints.VALUE_ANTIALIAS_ON);
		g.drawString(String.format("Pixelflut"), 2, 10);
		g.dispose();
	}

	synchronized public void draw() {
		drawOverlay();
		// Prepare composite buffer
		Graphics2D g = paintBuffer.createGraphics();
		g.drawImage(drawBuffer, 0, 0, null);
		g.setComposite(AlphaComposite.getInstance(AlphaComposite.SRC_OVER));
		g.drawImage(overlayBuffer, 0, 0, null);
		g.dispose();

		// Blit into double buffer
		Graphics g2 = strategy.getDrawGraphics();
		g2.drawImage(paintBuffer, 0, 0, null);
		g2.dispose();
		strategy.show();
	}

	public void setPixel(int x, int y, int argb) {
		// Below this values 8bit color channels are nulled.
		BufferedImage img = drawBuffer;

		if (x >= 0 && x < img.getWidth() && y >= 0 && y < img.getHeight()) {
			int alpha = (argb >>> 24) % 256;
			int rgb = argb & 0xffffff;

			if (alpha < 1)
				return;

			if (alpha < 255) {
				int target = img.getRGB(x, y);
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

	public int getPixel(int x, int y) {
		BufferedImage img = drawBuffer;
		if (x >= 0 && x < img.getWidth() && y >= 0 && y < img.getHeight())
			return img.getRGB(x, y) & 0x00ffffff;
		return 0;
	}

	public int getWidth() {
		return drawBuffer.getWidth();
	}

	public int getHeight() {
		return drawBuffer.getHeight();
	}

}
