#include "crow.h"
#include "WebService.h"

int main() {
    crow::SimpleApp app;
    WebService web_service;

    // 全局 CORS 处理
    CROW_CATCHALL_ROUTE(app)
    ([](const crow::request& req, crow::response& res){
        if (req.method == "OPTIONS"_method) {
            res.add_header("Access-Control-Allow-Origin", "*");
            res.add_header("Access-Control-Allow-Headers", "Content-Type");
            res.add_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
            res.end();
        }
    });

    // GET /api/scan_obj/{uid} - 获取scan_obj列表
    CROW_ROUTE(app, "/api/scan_obj/<string>")
    .methods("GET"_method)
    ([&web_service](const std::string& uid) {
        return web_service.get_scan_objs(uid);
    });

    // POST /api/scan_obj/{uid} - 添加新的scan_obj
    CROW_ROUTE(app, "/api/scan_obj/<string>")
    .methods("POST"_method)
    ([&web_service](const crow::request& req, const std::string& uid) {
        return web_service.add_scan_obj(uid, req);
    });

    // DELETE /api/scan_obj/{uid}/{id} - 删除指定id的表项
    CROW_ROUTE(app, "/api/scan_obj/<string>/<string>")
    .methods("DELETE"_method)
    ([&web_service](const std::string& uid, const std::string& id) {
        return web_service.delete_scan_obj(uid, id);
    });

    std::cout << "🚀 Web Service 已启动!" << std::endl;
    std::cout << "📍 服务地址: http://localhost:5071" << std::endl;
    
    app.port(5071).multithreaded().run();
    return 0;
}