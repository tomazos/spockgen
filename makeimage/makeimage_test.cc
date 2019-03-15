#include "makeimage/makeimage.h"

int main(int argc, char** argv) {
  std::vector<char*> args;
  for (int i = 0; i < argc; ++i) args.push_back(argv[i]);

  args.push_back((char*)"--width");
  args.push_back((char*)"100");
  args.push_back((char*)"--height");
  args.push_back((char*)"100");
  args.push_back((char*)"--filename");
  args.push_back((char*)"test.png");

  ImageMaker(args.size(), args.data())
      .render({0, 0}, {1, 1}, [](glm::vec2 pos) -> glm::vec4 {
        return {pos.x, pos.y, 1, 1};
      });
}
