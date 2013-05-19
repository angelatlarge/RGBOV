import java.awt.image.BufferedImage;
import java.io.File;
import javax.imageio.ImageIO;
import java.io.IOException;

public class ImageProcessor {
	
	public static void showUsage() {
		System.out.print(
		"ImageProcessor - prepare images for insertion into C programs\n\n" +
		"Usage: ImageProcess <path-to-image>\n"
		);
	}
	
	public static void execute(String[] args) {
		if (args.length <1) {
			System.err.printf("Error: no file argument specified\n\n");
			showUsage();
			return;
		}
		
		String filename = args[0];
		try {
			BufferedImage sourceImage = ImageIO.read( new File(filename) );
			ImageConverter ic = new ImageConverter(sourceImage, 7);
			System.out.println(ic.makeCArrayString(true, "graphic", "palette"));
		} catch (IOException e) {
			System.err.printf("Unable to read %s: %s", filename, e.getMessage());
			//~ e.printStackTrace();
		}
	}
	
	public static void main(String[] args) {
		execute(args);
	}		
}