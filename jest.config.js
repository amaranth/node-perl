module.exports = {
    collectCoverageFrom: [
        'test/**/*.{js,ts}',
    ],
    coverageThreshold: {
        global: {
            statements: 0,
            branches: 0,
            functions: 0,
            lines: 0,
        },
    },
    moduleDirectories: [
        'node_modules',
    ],
    moduleFileExtensions: [
        'ts',
        'js',
        'json',
    ],
    modulePaths: [
        '<rootDir>',
    ],
    setupFilesAfterEnv: [
        '<rootDir>/jest/test-bundler.jest.js',
    ],
    transform: {
        '^.+\\.(js|ts)$': '<rootDir>/jest/preprocessor.js',
    },
    testMatch: [
        '**/test/*.{ts,js}',
    ],
    modulePathIgnorePatterns: [
        'npm-cache',
        '.npm',
        '.yarn',
    ],
};
