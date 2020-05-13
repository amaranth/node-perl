declare class Perl {
    evaluate(input: string): unknown;
    use(lib: string): void;
    destroy(): void;
    getClass(className: string): unknown;
    static blessed(obj: object): string;
    call(...args: any[]): unknown;
}

export = Perl;
