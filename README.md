# spock
Spock is a C++ API to Vulkan [http://www.khonos.org/vulkan], developed by Andrew Tomazos [http://www.tomazos.com].

Spock is currently incomplete and under development.

Spock is an alternative to the standard `vulkan.hpp` binding that ships with Vulkan.  The goal is for Spock to be better than `vulkan.hpp` for certain use cases, with specifics TBD.

This repository consists of a few pieces:

1. `relaxngc` takes `registry.rnc` (the schema of `vk.xml`, the specification of the Vulkan C API) and autogenerates a schema for `vk.xml` called `vulkan_relaxng.h` and a parser for it.

2. `vulkan_api_schema_parser.h/cc` transforms the automatically generated schema into a hand-written schema in `vulkan_api_schema.h` in namespace `vks`.

3. `spock_api_schema.h` in namespace `sps` is a schema for the Spock C++ interface.  It is transformed from the C API specified in `vulkan_api_schema.h` by the `spock_api_schema_builder`.

4. `vkxmlc` then uses the generated Spock C++ API Schema to generate the Spock C++ API headers.

