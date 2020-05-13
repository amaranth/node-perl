declare class Perl {
    evaluate(input: string): unknown;
    use(lib: string): void;
    destroy(): void;
}

export = Perl;
