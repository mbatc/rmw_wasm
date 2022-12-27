#include <iostream> // REMOVE

#include "rmw_wasm_cpp/rmw_context_impl.hpp"

extern "C"
{
    rmw_context_impl_s::rmw_context_impl_s()
    {
        std::cout << "[WASM] Initializing context implementation";
        // TODO: log info
    }

    rmw_context_impl_s::~rmw_context_impl_s()
    {
        RMW_EMAIL_LOG_DEBUG("shutting down context implementation");
        // Shutdown middleware
        wasm_cpp::shutdown();

        // Make sure we didn't get destroyed while there's still a node
        if (0u != this->node_count) {
            RCUTILS_SAFE_FWRITE_TO_STDERR(
            "Not all nodes were finished before finishing the context\n."
            "Ensure `rmw_destroy_node` is called for all nodes before `rmw_context_fini`,"
            "to avoid leaking.\n");
        }
    }

    rmw_ret_t rmw_context_impl_s::init(
        [[maybe_unused]] rmw_init_options_t * options, 
        [[maybe_unused]] size_t domain_id)
    {
        // If this isn't the first node, just increment the node counter;
        // we only do the rest if this was the first node
        std::scoped_lock<std::mutex> lock(mutex_initialization);
        if (0u != this->node_count) {
            this->node_count++;
            return RMW_RET_OK;
        }

        this->graph_guard_condition = rmw_wasm_cpp::create_guard_condition();
        if (nullptr == this->graph_guard_condition) {
            cleanup();
            return RMW_RET_BAD_ALLOC;
        }

        // Init middleware
        try {
            wasm_cpp::init();
        } catch (const std::runtime_error & e) {
            RMW_SET_ERROR_MSG(e.what());
            cleanup();
            return RMW_RET_ERROR;
        }

        this->node_count++;
        return RMW_RET_OK;
    }

    rmw_ret_t rmw_context_impl_s::fini()
    {
        std::scoped_lock<std::mutex> lock(mutex_initialization);
        if (0u != --this->node_count) {
            return RMW_RET_OK;
        }

        cleanup();
        return RMW_RET_OK;
    }

    void rmw_context_impl_s::cleanup()
    {
        if (this->graph_guard_condition) {
            rmw_wasm_cpp::destroy_guard_condition(this->graph_guard_condition);
        }
    }

}