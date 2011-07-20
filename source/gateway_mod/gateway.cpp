#include "gateway.h"
#include "type.pb.h"

#include <boost/thread/locks.hpp>
#include <ctime>
#include <iostream>
#include <google/protobuf/descriptor.h>
#include "interface.h"


Worker::Worker(Pool &){
    register_api("/query", boost::bind(&Worker::handle, this, _1, _2), "traite les requétes");
    register_api("/load", boost::bind(&Worker::load, this, _1, _2), "traite les requétes");
    register_api("/register", boost::bind(&Worker::register_navitia, this, _1, _2), "ajout d'un NAViTiA au pool");
    register_api("/status", boost::bind(&Worker::status, this, _1, _2), "ajout d'un NAViTiA au pool");

}


webservice::ResponseData Worker::status(webservice::RequestData& request, Pool& pool){
    webservice::ResponseData rd;
    Context context;

    render_status(request, rd, context, pool);

    return rd;
}

webservice::ResponseData Worker::register_navitia(webservice::RequestData& request, Pool& pool){
    webservice::ResponseData rd;
    
    if(request.params.find("url") == request.params.end()){
        rd.status_code = 500;
        return rd;
    }

    //TODO valider l'url
    pool.add_navitia(new Navitia(request.params["url"], 8));
    

    return status(request, pool);
}

webservice::ResponseData Worker::handle(webservice::RequestData& request, Pool& pool){
    webservice::ResponseData rd;
    Context context;
    dispatcher(request, rd, pool, context);
    render(request, rd, context);
    return rd;
}

webservice::ResponseData Worker::load(webservice::RequestData& request, Pool& pool){
    //TODO gestion de la desactivation
    BOOST_FOREACH(Navitia* nav, pool.navitia_list){
        nav->load();
    }
    return this->status(request, pool);
}

void Dispatcher::operator()(webservice::RequestData& request, webservice::ResponseData& response, Pool& pool, Context& context){
    Navitia* nav =  pool.next();
    std::cout << request.path << std::endl;
    std::cout << nav->url << " - " << nav->unused_thread << std::endl;

    std::pair<int, std::string> res;
    try{
        res = nav->query(request.path.substr(request.path.find_last_of('/')) + "?" + request.raw_params);
        pool.release_navitia(nav);
    }catch(long& code){
        pool.release_navitia(nav);
        response.status_code = code;
        return;
    }
    std::unique_ptr<pbnavitia::PTRefResponse> resp(new pbnavitia::PTRefResponse());
    if(resp->ParseFromString(res.second)){
        context.pb = std::move(resp);
        context.service = Context::PTREF;
    }else{
        context.str = res.second;
        context.service = Context::BAD_RESPONSE;
        response.status_code = res.first;
    }
}




MAKE_WEBSERVICE(Pool, Worker)
