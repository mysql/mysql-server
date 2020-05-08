var profile = (function(){
    return {
        layerOptimize: "closure",
        releaseDir: "../../../release",

        packages: [{
            name: "dojo",
            location: "../../../dojo"
        }],

        defaultConfig: {
            async: 1
        },

        dojoBootText: "require.boot && require.apply(null, require.boot);",

        staticHasFeatures: {
            'dom': 1,
            'host-browser': 1,
            'dojo-inject-api': 1,
            'dojo-loader-eval-hint-url': 1,
            'dojo-built': 1,
            'host-node': 0,
            'host-rhino': 0,
            'dojo-trace-api': 0,
            'dojo-sync-loader': 0,
            'dojo-config-api': 1,
            'dojo-cdn': 0,
            'dojo-sniff': 0,
            'dojo-requirejs-api': 0,
            'dojo-test-sniff': 0,
            'dojo-combo-api': 0,
            'dojo-undef-api': 0,
            'config-tlmSiblingOfDojo': 0,
            'config-dojo-loader-catches': 0,
            'config-stripStrict': 0,
            'dojo-timeout-api': 0,
            'dojo-dom-ready-api': 0,
            'dojo-log-api': 0,
            'dojo-amd-factory-scan': 0,
            'dojo-publish-privates': 0
        },

        layers: {
            "dojo/dojo": {
                include: [],
                customBase: 1
            }
        }
    };
})();
