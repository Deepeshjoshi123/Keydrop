#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "keydrop/schema/schema_config.hpp"

using namespace keydrop;

namespace {
bool read_file(const std::string& path, std::string& out) { std::ifstream f(path); if (!f) return false; std::ostringstream s; s << f.rdbuf(); out = s.str(); return true; }
bool write_file(const std::string& path, const std::string& text) { std::ofstream f(path); f << text; return f.good(); }
const ConfiguredSchema* find_schema(const std::vector<ConfiguredSchema>& schemas, const std::string& name) { for (const auto& s : schemas) if (s.schema.schema_name == name) return &s; return nullptr; }
bool load(const std::string& path, std::vector<ConfiguredSchema>& schemas) { const auto r = SchemaConfig::load_yaml_file(path, schemas); if (!r.ok()) std::cerr << "configuration error: " << r.message << "\n"; return r.ok(); }
bool load_payload(const std::string& path, JsonObject& out) { std::string text; if (!read_file(path, text)) { std::cerr << "cannot read " << path << "\n"; return false; } const auto r = parse_json_object(text, out); if (!r.ok()) std::cerr << "payload error: " << r.message << "\n"; return r.ok(); }
std::string hex(const Buffer& buffer) { std::ostringstream o; o << std::hex << std::setfill('0'); for (byte b : buffer.data()) o << std::setw(2) << static_cast<unsigned>(b); return o.str(); }
bool unhex(const std::string& text, Buffer& out) { if (text.size() % 2) return false; for (usize i=0;i<text.size();i+=2) { try { out.write(static_cast<byte>(std::stoul(text.substr(i,2), nullptr, 16))); } catch (...) { return false; } } return true; }
void usage() { std::cerr << "Usage:\n  keydrop_cli validate <schema.yaml>\n  keydrop_cli inspect <schema.yaml> <schema>\n  keydrop_cli example <schema.yaml> <schema>\n  keydrop_cli init --from-json <payload.json> <schema-name> <output.yaml>\n  keydrop_cli encode <schema.yaml> <schema> <payload.json>\n  keydrop_cli decode <schema.yaml> <packet.hex>\n  keydrop_cli compatibility <local.yaml> <schema> <peer.yaml> <schema>\n"; }
}

int main(int argc, char** argv) {
    if (argc < 3) { usage(); return 2; }
    const std::string command = argv[1];
    if (command == "init" && argc == 6 && std::string(argv[2]) == "--from-json") {
        JsonObject object; if (!load_payload(argv[3], object)) return 1;
        std::ostringstream yaml; yaml << "keydrop: 1\n\nschemas:\n  " << argv[4] << ":\n    id: 1\n    version: 1\n    profile: telemetry-balanced\n    fields:\n";
        for (const auto& value : object) { std::string type = value.second.type == JsonValueType::string ? "string" : value.second.type == JsonValueType::bytes ? "bytes" : value.second.type == JsonValueType::decimal ? "f64" : "i32"; yaml << "      - key: " << value.first << "\n        type: " << type << "\n"; }
        if (!write_file(argv[5], yaml.str())) { std::cerr << "cannot write " << argv[5] << "\n"; return 1; } return 0;
    }
    if (command == "validate" && argc == 3) { std::vector<ConfiguredSchema> s; return load(argv[2], s) ? 0 : 1; }
    if (command == "inspect" && argc == 4) { std::vector<ConfiguredSchema> s; if (!load(argv[2],s)) return 1; auto schema=find_schema(s,argv[3]); if(!schema){std::cerr<<"unknown schema: "<<argv[3]<<"\n";return 1;} std::cout<<SchemaConfig::generate_markdown(*schema); return 0; }
    if (command == "example" && argc == 4) { std::vector<ConfiguredSchema> s; if (!load(argv[2],s)) return 1; auto schema=find_schema(s,argv[3]); if(!schema){std::cerr<<"unknown schema: "<<argv[3]<<"\n";return 1;} std::cout<<SchemaConfig::generate_example_json(*schema); return 0; }
    if (command == "compatibility" && argc == 6) { std::vector<ConfiguredSchema>a,b; if(!load(argv[2],a)||!load(argv[4],b))return 1; auto x=find_schema(a,argv[3]),y=find_schema(b,argv[5]); if(!x||!y){std::cerr<<"schema not found\n";return 1;} auto r=SchemaConfig::check_compatible(*x,*y); if(!r.ok()){std::cerr<<r.message<<"\n";return 1;} std::cout<<"compatible\n";return 0; }
    if (command == "encode" && argc == 5) { std::vector<ConfiguredSchema>s; if(!load(argv[2],s))return 1;auto schema=find_schema(s,argv[3]);if(!schema){std::cerr<<"unknown schema\n";return 1;}JsonObject j;if(!load_payload(argv[4],j))return 1;SchemaRuntime runtime;register_configured_schema(runtime,*schema);apply_configured_profile(runtime,*schema);Buffer b;auto sent=runtime.send_json(schema->schema.schema_name,j,b);if(!sent.ok()){std::cerr<<sent.message<<"\n";return 1;}std::cout<<hex(b)<<"\n";return 0; }
    if (command == "decode" && argc == 4) { std::vector<ConfiguredSchema>s; if(!load(argv[2],s))return 1;Buffer b;if(!unhex(argv[3],b)){std::cerr<<"invalid packet hex\n";return 1;}SchemaRuntime runtime;for(const auto&x:s){register_configured_schema(runtime,x);apply_configured_profile(runtime,x);}std::string name;JsonObject j;auto r=runtime.receive_json(b,name,j);if(!r.ok()){std::cerr<<r.message<<"\n";return 1;}std::cout<<format_json_object(j)<<"\n";return 0; }
    usage(); return 2;
}
