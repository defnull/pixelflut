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
import java.awt.event.MouseEvent;
import java.awt.event.MouseMotionListener;
import java.awt.image.BufferStrategy;
import java.awt.image.BufferedImage;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import javax.imageio.ImageIO;
import javax.swing.JFrame;

public class NetCanvas implements ComponentListener, KeyListener,
		MouseMotionListener {
	private volatile BufferedImage pxBuffer;
	private volatile BufferStrategy strategy;
	private final Thread refresher;
	private final JFrame frame;
	private final Canvas canvas;
	private final List<Drawable> drawables = new ArrayList<>();
	private long lastDraw;

	public NetCanvas() {
		frame = new JFrame();
		canvas = new Canvas();
		resizeBuffer(1024, 1024);

		frame.addComponentListener(this);
		canvas.addKeyListener(this);
		canvas.addMouseMotionListener(this);

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

		refresher = new Thread(new Runnable() {
			@Override
			public void run() {
				lastDraw = System.currentTimeMillis() - 1;
				try {
					while (true) {
						final long d1 = System.currentTimeMillis();
						draw(d1 - lastDraw);
						final long d2 = System.currentTimeMillis();
						lastDraw = d1;
						Thread.sleep(Math.max(1, (1000 / 30) - (d2 - d1)));
					}
				} catch (final InterruptedException e) {
				}
			}
		});
		refresher.start();
	}

	public void saveAs(final File file) throws IOException {
		ImageIO.write(pxBuffer, "png", file);
	}

	public void loadFrom(final File file) throws IOException {
		final BufferedImage img = ImageIO.read(file);
		resizeBuffer(img.getWidth(), img.getHeight());
		blit(img, pxBuffer);
	}

	private void blit(final BufferedImage source, final BufferedImage target) {
		final Graphics g = target.getGraphics();
		g.drawImage(source, 0, 0, null);
		g.dispose();
	}

	synchronized private void resizeBuffer(final int w, final int h) {
		BufferedImage newBuffer;
		int mw = w, mh = h;

		if (pxBuffer != null) {
			mw = Math.max(w, pxBuffer.getWidth());
			mh = Math.max(h, pxBuffer.getHeight());
			if (mw > w && mh > h) {
				return;
			}
		}

		newBuffer = frame.getGraphicsConfiguration().createCompatibleImage(w,
				h, Transparency.OPAQUE);
		if (pxBuffer != null) {
			blit(pxBuffer, newBuffer);
		}
		pxBuffer = newBuffer;

		frame.getGraphicsConfiguration().createCompatibleImage(w, h,
				Transparency.OPAQUE);
	}

	private void drawOverlay(final Graphics2D g, final long dt) {
		g.setComposite(AlphaComposite.SrcOver);
		g.setRenderingHint(RenderingHints.KEY_ANTIALIASING,
				RenderingHints.VALUE_ANTIALIAS_ON);
		g.drawString(String.format("Pixelflut FPS:%.2f", 1000.0 / dt), 2, 10);

		synchronized (drawables) {
			final Iterator<Drawable> i = drawables.iterator();
			while (i.hasNext()) {
				final Drawable d = i.next();
				if (d.isAlive()) {
					d.tick(dt);
					d.draw(g);
				} else {
					d.dispose();
					i.remove();
				}
			}
		}
	}

	synchronized public void draw(final long dt) {
		if (!frame.isVisible()) {
			return;
		}
		final Graphics2D g = (Graphics2D) strategy.getDrawGraphics();
		g.drawImage(pxBuffer, 0, 0, null);
		drawOverlay(g, dt);
		g.dispose();
		strategy.show();
	}

	public void setPixel(final int x, final int y, final int argb) {
		// Below this values 8bit color channels are nulled.
		final BufferedImage img = pxBuffer;

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
		final BufferedImage img = pxBuffer;
		if (x >= 0 && x < img.getWidth() && y >= 0 && y < img.getHeight()) {
			return img.getRGB(x, y) & 0x00ffffff;
		}
		return 0;
	}

	public int getWidth() {
		return pxBuffer.getWidth();
	}

	public int getHeight() {
		return pxBuffer.getHeight();
	}

	@Override
	public void keyReleased(final KeyEvent e) {
		if (e.getKeyChar() == 'c') {
			final Graphics g = pxBuffer.getGraphics();
			g.setColor(Color.BLACK);
			g.fillRect(0, 0, pxBuffer.getWidth(), pxBuffer.getHeight());
			g.dispose();
		} else if (e.getKeyChar() == 'q'
				|| e.getKeyCode() == KeyEvent.VK_ESCAPE) {
			System.exit(0);
		} else if (e.getKeyChar() == 'l') {
			Label.show = !Label.show;
		} else if (e.getKeyChar() == 's') {
			try {
				saveAs(new File("/tmp/canvas.png"));
			} catch (final IOException e1) {
				e1.printStackTrace();
			}
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

	@Override
	public void mouseDragged(final MouseEvent e) {
		// System.err.println(e.getPoint());
		// setPixel(e.getX(), e.getY(), 0xffffffff);
		final Graphics2D g = (Graphics2D) pxBuffer.getGraphics();
		g.setColor(Color.MAGENTA);
		g.fillOval(e.getX() - 50, e.getY() - 50, 100, 100);
		g.dispose();
	}

	@Override
	public void mouseMoved(final MouseEvent e) {
		// TODO Auto-generated method stub

	}

	public void addDrawable(final Label label) {
		synchronized (drawables) {
			drawables.add(label);
		}
	}

}
