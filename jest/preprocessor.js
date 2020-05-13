const tsc = require('typescript');
const tsConfig = require('../tsconfig.json');

module.exports = {
    process(src, path, ...rest) {
        const jestTsConfig = Object.assign({}, tsConfig.compilerOptions, {
            module: tsc.ModuleKind.CommonJS,
            sourceMap: false,
            inlineSourceMap: true,
        })
        return tsc.transpile(
            src,
            jestTsConfig,
            path,
            [],
        )
    },
};
