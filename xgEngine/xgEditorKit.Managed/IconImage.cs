// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // Prepares an RGBA8 image (straight alpha, rows top-down) for use as a small editor icon:
    // - an image without any transparent pixel gets its background removed: the colour of the four corners (when they
    //   agree), flood-filled from the edges, becomes transparent, and the pixels bordering it get a soft alpha with the
    //   background colour taken back out (anti-aliased edges against white stay clean). Enclosed areas of that colour stay.
    // - images larger than maxSize are shrunk with an alpha-weighted area filter (no jagged edges or dark fringes when
    //   drawn at 13-28 px without mipmaps).
    // - transparent pixels take the colour of their visible neighbours, so bilinear filtering doesn't pull in black.
    public static class IconImage
    {
        public const int DefaultMaxSize = 64;
        private const int BackgroundTolerance = 40;
        private const int EdgeRange = 160;

        public static byte[] Prepare(byte[] rgba, ref int width, ref int height, int maxSize = DefaultMaxSize)
        {
            if (rgba.Length < width * height * 4 || width <= 0 || height <= 0)
                return rgba;
            byte[] pixels = (byte[])rgba.Clone();
            if (IsOpaque(pixels))
                RemoveBackground(pixels, width, height);
            if (width > maxSize || height > maxSize)
                pixels = Shrink(pixels, ref width, ref height, maxSize);
            Bleed(pixels, width, height);
            return pixels;
        }

        public static bool IsOpaque(byte[] rgba)
        {
            for (int i = 3; i < rgba.Length; i += 4)
                if (rgba[i] != 255)
                    return false;
            return true;
        }

        // Returns false when the corners don't share one colour (no background to remove).
        public static bool RemoveBackground(byte[] rgba, int width, int height)
        {
            int[] corners = { 0, width - 1, (height - 1) * width, height * width - 1 };
            int bg = corners[0];
            foreach (int corner in corners)
                if (Distance(rgba, corner * 4, rgba, bg * 4) > BackgroundTolerance)
                    return false;
            byte br = rgba[bg * 4], bgG = rgba[bg * 4 + 1], bb = rgba[bg * 4 + 2];

            bool[] background = new bool[width * height];
            Queue<int> queue = new();
            void Seed(int x, int y)
            {
                int i = y * width + x;
                if (!background[i] && DistanceTo(rgba, i * 4, br, bgG, bb) <= BackgroundTolerance)
                {
                    background[i] = true;
                    queue.Enqueue(i);
                }
            }
            for (int x = 0; x < width; x++)
            {
                Seed(x, 0);
                Seed(x, height - 1);
            }
            for (int y = 0; y < height; y++)
            {
                Seed(0, y);
                Seed(width - 1, y);
            }
            while (queue.Count > 0)
            {
                int i = queue.Dequeue();
                int x = i % width, y = i / width;
                if (x > 0) Seed(x - 1, y);
                if (x < width - 1) Seed(x + 1, y);
                if (y > 0) Seed(x, y - 1);
                if (y < height - 1) Seed(x, y + 1);
            }

            for (int i = 0; i < background.Length; i++)
            {
                int p = i * 4;
                if (background[i])
                {
                    rgba[p + 3] = 0;
                    continue;
                }
                if (!TouchesBackground(background, i, width, height))
                    continue;
                int distance = DistanceTo(rgba, p, br, bgG, bb);
                if (distance >= EdgeRange)
                    continue;
                float alpha = Math.Clamp((float)(distance - BackgroundTolerance * 0.5f) / (EdgeRange - BackgroundTolerance * 0.5f), 0.0f, 1.0f);
                if (alpha <= 0.0f)
                {
                    rgba[p + 3] = 0;
                    continue;
                }
                rgba[p] = Unblend(rgba[p], br, alpha);
                rgba[p + 1] = Unblend(rgba[p + 1], bgG, alpha);
                rgba[p + 2] = Unblend(rgba[p + 2], bb, alpha);
                rgba[p + 3] = (byte)MathF.Round(alpha * 255.0f);
            }
            return true;
        }

        private static bool TouchesBackground(bool[] background, int i, int width, int height)
        {
            int x = i % width, y = i / width;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                {
                    int nx = x + dx, ny = y + dy;
                    if ((dx != 0 || dy != 0) && nx >= 0 && ny >= 0 && nx < width && ny < height && background[ny * width + nx])
                        return true;
                }
            return false;
        }

        private static byte Unblend(byte value, byte background, float alpha) =>
            (byte)Math.Clamp(MathF.Round((value - (1.0f - alpha) * background) / alpha), 0.0f, 255.0f);

        private static int Distance(byte[] a, int ia, byte[] b, int ib) =>
            Math.Max(Math.Abs(a[ia] - b[ib]), Math.Max(Math.Abs(a[ia + 1] - b[ib + 1]), Math.Abs(a[ia + 2] - b[ib + 2])));

        private static int DistanceTo(byte[] a, int ia, byte r, byte g, byte b) =>
            Math.Max(Math.Abs(a[ia] - r), Math.Max(Math.Abs(a[ia + 1] - g), Math.Abs(a[ia + 2] - b)));

        // Area resampling in premultiplied alpha, so transparent pixels don't darken the result.
        public static byte[] Shrink(byte[] rgba, ref int width, ref int height, int maxSize)
        {
            float scale = (float)maxSize / Math.Max(width, height);
            int newWidth = Math.Max(1, (int)MathF.Round(width * scale));
            int newHeight = Math.Max(1, (int)MathF.Round(height * scale));
            float[] premultiplied = new float[width * height * 4];
            for (int i = 0; i < width * height; i++)
            {
                float a = rgba[i * 4 + 3] / 255.0f;
                premultiplied[i * 4] = rgba[i * 4] * a;
                premultiplied[i * 4 + 1] = rgba[i * 4 + 1] * a;
                premultiplied[i * 4 + 2] = rgba[i * 4 + 2] * a;
                premultiplied[i * 4 + 3] = rgba[i * 4 + 3];
            }
            float[] rows = ResampleAxis(premultiplied, width, height, newWidth, true);
            float[] both = ResampleAxis(rows, newWidth, height, newHeight, false);
            byte[] result = new byte[newWidth * newHeight * 4];
            for (int i = 0; i < newWidth * newHeight; i++)
            {
                float a = both[i * 4 + 3];
                result[i * 4 + 3] = (byte)Math.Clamp(MathF.Round(a), 0.0f, 255.0f);
                if (a <= 0.0f)
                    continue;
                float inverse = 255.0f / a;
                for (int c = 0; c < 3; c++)
                    result[i * 4 + c] = (byte)Math.Clamp(MathF.Round(both[i * 4 + c] * inverse), 0.0f, 255.0f);
            }
            width = newWidth;
            height = newHeight;
            return result;
        }

        private static float[] ResampleAxis(float[] source, int width, int height, int size, bool horizontal)
        {
            int length = horizontal ? width : height;
            int lines = horizontal ? height : width;
            int outWidth = horizontal ? size : width;
            float[] result = new float[outWidth * (horizontal ? height : size) * 4];
            float step = (float)length / size;
            for (int line = 0; line < lines; line++)
                for (int o = 0; o < size; o++)
                {
                    float start = o * step, end = start + step;
                    float r = 0, g = 0, b = 0, a = 0;
                    for (int s = (int)start; s < Math.Min((int)MathF.Ceiling(end), length); s++)
                    {
                        float weight = MathF.Min(end, s + 1) - MathF.Max(start, s);
                        int i = (horizontal ? line * width + s : s * width + line) * 4;
                        r += source[i] * weight;
                        g += source[i + 1] * weight;
                        b += source[i + 2] * weight;
                        a += source[i + 3] * weight;
                    }
                    int d = (horizontal ? line * outWidth + o : o * outWidth + line) * 4;
                    result[d] = r / step;
                    result[d + 1] = g / step;
                    result[d + 2] = b / step;
                    result[d + 3] = a / step;
                }
            return result;
        }

        // Gives fully transparent pixels the average colour of their visible neighbours (a few passes), keeping alpha.
        public static void Bleed(byte[] rgba, int width, int height, int passes = 4)
        {
            bool[] known = new bool[width * height];
            for (int i = 0; i < known.Length; i++)
                known[i] = rgba[i * 4 + 3] > 0;
            List<int> filled = new();
            for (int pass = 0; pass < passes; pass++)
            {
                filled.Clear();
                for (int y = 0; y < height; y++)
                    for (int x = 0; x < width; x++)
                    {
                        int i = y * width + x;
                        if (known[i])
                            continue;
                        int r = 0, g = 0, b = 0, n = 0;
                        for (int dy = -1; dy <= 1; dy++)
                            for (int dx = -1; dx <= 1; dx++)
                            {
                                int nx = x + dx, ny = y + dy;
                                if (nx < 0 || ny < 0 || nx >= width || ny >= height || !known[ny * width + nx])
                                    continue;
                                int j = (ny * width + nx) * 4;
                                r += rgba[j];
                                g += rgba[j + 1];
                                b += rgba[j + 2];
                                n++;
                            }
                        if (n == 0)
                            continue;
                        rgba[i * 4] = (byte)(r / n);
                        rgba[i * 4 + 1] = (byte)(g / n);
                        rgba[i * 4 + 2] = (byte)(b / n);
                        filled.Add(i);
                    }
                if (filled.Count == 0)
                    break;
                foreach (int i in filled)
                    known[i] = true;
            }
        }
    }
}
