export default {
    logo: <span>MONICA Docs</span>,
    project: {
        link: 'https://github.com/AlfredoDompablo/WEB_MONICA.git',
    },
    docsRepositoryBase: 'https://github.com/AlfredoDompablo/WEB_MONICA/tree/main',
    footer: {
        text: 'Monica Project Documentation',
    },
    useNextSeoProps() {
        return {
            titleTemplate: '%s – Monica Docs'
        }
    }
}
