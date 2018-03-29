package de.paws.pixelwar;

import java.awt.Graphics2D;

public abstract class Drawable {

	private boolean alive = true;

	public final boolean isAlive() {
		return alive;
	}

	public final void setAlive(final boolean state) {
		alive = state;
	}

	public void tick(final long dt) {
	}

	public void dispose() {
	}

	public abstract void draw(final Graphics2D g);

}
