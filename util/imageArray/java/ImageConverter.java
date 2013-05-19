import java.awt.image.BufferedImage;
import java.util.LinkedHashMap;
import java.lang.IllegalArgumentException;

/**
 * ImageConverter - converts an image to a palettized image 
 *                  and prints it out as a C/C++ array
 * 
 * The image conversion takes place in two steps:
 * 
 * First all the colors and their usage count is 
 * stored in a hash table. Using this hash table we 
 * can quantize the colors if the number of used colors
 * exceeds the palette size (currently not done)
 *
 * Second, every color reference is converted to a 
 * palette index
 * 
 */ 
public class ImageConverter {
	private final Palette mPalette;
	private final BufferedImage mSourceImage;
	
	private class Palette {
		private final LinkedHashMap<Integer, Integer> mUsedColors = new LinkedHashMap<Integer, Integer>();
		private final int mIntensitiesPerChannel;
		private int mCount = 0;
		private boolean indecesReady = false;
		
		public Palette(int intensitiesPerChannel, int[] forcedColors) {
			mIntensitiesPerChannel = intensitiesPerChannel;
			if (forcedColors != null)
				for (int color : forcedColors) {
					if (mUsedColors.get(color) == null) 
						mCount++;
					mUsedColors.put(color, -1);
				}
		}

		private int reduceColor(int sourceColor) {
			if (mIntensitiesPerChannel<255) {
				int outputColor = 0;
				for (int idxChannel=0; idxChannel<3; idxChannel++) {
					int chanInData = (sourceColor & (0xFF << (8*idxChannel))) >> (8*idxChannel);
					int chanOutData = 
						(int)(chanInData/(0xFF/(float)mIntensitiesPerChannel))
					//~ *(mIntensitiesPerChannel)
						;
					outputColor += chanOutData << (8*idxChannel);
				}
				return outputColor;
			} else {
				return sourceColor;
			}
		}
		
		public void foundColor(int sourceColor) {
			if (indecesReady) {
				throw new IllegalArgumentException("Already generated indeces");
			}
			
			// Reduce the color according to the intensities per channel setting
			int outputColor = reduceColor(sourceColor);
			
			// Update the color usage count
			Integer useCount = mUsedColors.get(outputColor);
			if (useCount==null) {
				mUsedColors.put(outputColor, 1);
				mCount++;
			} else {
				if (useCount != -1) 
					mUsedColors.put(outputColor, useCount+1);
			}
		}
		
		public int getColor(int sourceColor) {
			return reduceColor(sourceColor);
		}
		
		private void createIndeces() {
			if (indecesReady) 
				return;
			int paletteIndex = 0;
			for (int color : mUsedColors.keySet() ) {
				mUsedColors.put(color, paletteIndex++);
			}
			indecesReady = true;
		}
		
		public int getColorIndex(int sourceColor) {
			int reducedColor = reduceColor(sourceColor);
			if (!indecesReady) 
				createIndeces();
			return mUsedColors.get(reducedColor);
		}
		
		public String makeCArrayString(String dsName) {
			StringBuilder sb = new StringBuilder();
			sb.append(String.format("#define PALETTE_SIZE\t%d\n", mCount));
			sb.append(String.format("uint8_t %s[PALETTE_SIZE * 3] = {\n", dsName));
			int[] channelData = new int[3];
			int idxColor = 0;
			for (int color : mUsedColors.keySet() ) {
				for (int idxChannel = 0; idxChannel<3; idxChannel++) {
					channelData[idxChannel] = (color & (0xFF << (8*idxChannel))) >> (8*idxChannel);
				}
				sb.append(String.format("\t\t%s0x%02x,0x%02x,0x%02x\n", (idxColor++>0)?",":" ", channelData[0], channelData[1], channelData[2]));
			}
			sb.append("\t};\n");
			return sb.toString();
		}
		
	}
	
	public ImageConverter(BufferedImage sourceImage, int intensitiesPerChannel) {
		mPalette = new Palette(intensitiesPerChannel, null);
		mSourceImage = sourceImage;
		
		for ( int y=0 ; y<sourceImage.getHeight(); y++) {
			for ( int x=0 ; x<sourceImage.getWidth(); x++) {
				int sourceColor = sourceImage.getRGB(x, y);
				mPalette.foundColor(sourceColor);
			}
		}
	}
	
	public String makeCArrayString(boolean dataYfirst, String dataName, String paletteName) {
		StringBuilder sb = new StringBuilder();

		sb.append(String.format("\n#define GRAPHIC_WIDTH	%d\n", mSourceImage.getWidth()));
		sb.append(String.format("\n#define GRAPHIC_HEIGHT	%d\n\n", mSourceImage.getHeight()));
		
		// Print the palette
		sb.append(mPalette.makeCArrayString(paletteName));

		sb.append(String.format("uint8_t %s[GRAPHIC_WIDTH][GRAPHIC_HEIGHT] = \n{\n", dataName));
		
		// Print the data itself
		int[] limits = {0,0};
		limits[dataYfirst?0:1] = mSourceImage.getWidth();
		limits[dataYfirst?1:0] = mSourceImage.getHeight();
		for (int i=0; i<limits[0]; i++) {
			for (int j=0; j<limits[1]; j++) {
				if (j==0) {
					sb.append("{");
				} else {
					sb.append(",");
				}
				sb.append(String.format("0x%02x", mPalette.getColorIndex(mSourceImage.getRGB(dataYfirst?i:j,dataYfirst?j:i))));
			}
			
			if (i<limits[0]) {
				sb.append("},\n");
			} else {
				sb.append("}\n");
			}
		}
		
		sb.append(String.format("};\n", dataName));
		
		return sb.toString();
	}
}