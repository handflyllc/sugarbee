module_loads = (module_loads or 0) + 1
return {loads = module_loads, status = "Module cache shared across files"}
