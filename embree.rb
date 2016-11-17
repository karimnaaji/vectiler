class Embree < Formula
  homepage "http://embree.github.io/"
  url "https://github.com/embree/embree/releases/download/v2.7.0/embree-2.7.0.x86_64.macosx.tar.gz"
  sha256 "2e526a84bb2483fb72fbd0fe319b8463ce0cc766"

  def install
    lib.install Dir["lib/libembree.2.dylib"]
    include.install Dir["include/*"]
    system "ln", "-s", (lib/"libembree.2.dylib"), (lib/"libembree.dylib")
  end

end
